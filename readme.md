    # Mobile IPv6 (MIPv6) Testbed Setup Guide on Fedora 23

This repository contains the complete step-by-step setup and configuration guide for deploying a **Mobile IPv6 (MIPv6)** experimental testbed using virtual machines on **Fedora 23 Workstation**.

---

## 📋 Table of Contents
1. [Overview & Network Topology](#1-overview--network-topology)
2. [Prerequisites & OS Installation](#2-prerequisites--os-installation)
3. [Package Installation](#3-package-installation)
4. [Node Configurations](#4-node-configurations)
   - [Home Agent (HA)](#home-agent-ha)
   - [Foreign Router (Router)](#foreign-router-router)
   - [Correspondent Node (CN)](#correspondent-node-cn)
   - [Mobile Node (MN)](#mobile-node-mn)
5. [Baseline Connectivity Verification](#5-baseline-connectivity-verification)
6. [Executing MIPv6 & Handoff Demonstration](#6-executing-mipv6--handoff-demonstration)
7. [Cross-Check & Troubleshooting Notes](#7-cross-check--troubleshooting-notes)

---

## 1. Overview & Network Topology

The lab topology simulates a Mobile Node moving between its **Home Network** and a **Foreign Network** while maintaining continuous IPv6 communication with a **Correspondent Node**.

### Network Map

```
  [ Home Agent (HA) ] <--- Network-1 (2001:db8:1::/64) ---> [ Mobile Node (MN) ]
          |                                                      (Home Link)
  Network-2 (2001:db8:2::/64)                                        |
          |                                                       [ Switch Link ]
  [ Foreign Router ]  <--- Network-3 (2001:db8:3::/64) ---> [ Mobile Node (MN) ]
          |                                                     (Foreign Link)
          +--------------> [ Correspondent Node (CN) ]
```

### Virtual Machine Network Adapters Setup

| Virtual Machine | Adapter 1 (enp0s3) | Adapter 2 (enp0s8) | Subnet / Role |
| :--- | :--- | :--- | :--- |
| **Home-Agent (HA)** | Internal Network: `Network-1` | Internal Network: `Network-2` | HA / Home Link Gateway (`2001:db8:1::1`, `2001:db8:2::1`) |
| **Router** | Internal Network: `Network-2` | Internal Network: `Network-3` | Foreign Router (`2001:db8:2::2`, `2001:db8:3::1`) |
| **Correspondent-Node (CN)** | Internal Network: `Network-3` | *(Disabled)* | Static Node (`2001:db8:3::2`) |
| **Mobile-Node (MN)** | Internal Network: `Network-1` *(initial)* | *(Disabled)* | Mobile Node (`2001:db8:1::100` HoA) |

---

## 2. Prerequisites & OS Installation

* **Operating System ISO:** `Fedora-Live-Workstation-x86_64-23-10.iso`
* **Download URL:** [Fedora Archive (Fedora 23 Workstation x86_64)](https://archives.fedoraproject.org/pub/archive/fedora/linux/releases/23/Workstation/x86_64/iso/)
* **Hypervisor:** VirtualBox / VMware Workstation

> **Note on Administrative Privileges:**
> Legacy Fedora Live ISOs do not automatically add the primary user to the `wheel` (sudoers) group. Perform the following on **all 4 VMs** immediately after installation while NAT network adapter is temporarily enabled:

```bash
# Switch to root account
su -

# Add your user to the administrative wheel group
usermod -aG wheel your_username

# Exit root and restart the OS to apply group permissions
exit
reboot
```

---

## 3. Package Installation

Connect all VMs to NAT temporarily to install the required packages.

### 3.1 Install MIPv6 Daemon (`mip6d`) on HA, MN, and CN

Run on **HA**, **MN**, and **CN**:

```bash
# Download mipv6-daemon RPM package
wget https://linuxsoft.cern.ch/cern/centos/7/updates/x86_64/Packages/Packages/mipv6-daemon-1.0-5.el7.x86_64.rpm -O ~/mipv6-daemon-1.0-5.el7.x86_64.rpm

# Alternate link if primary link is unavailable:
# wget http://vault.centos.org/7.1.1503/cr/x86_64/Packages/mipv6-daemon-1.0-5.el7.x86_64.rpm -O ~/mipv6-daemon-1.0-5.el7.x86_64.rpm

# Install via DNF
sudo dnf install -y ~/mipv6-daemon-1.0-5.el7.x86_64.rpm

# Verify installation
mip6d -V
```

### 3.2 Install `radvd` and Network Tools on HA and Router

Run on **HA** and **Router**:

```bash
sudo dnf install -y radvd net-tools iproute wget

# Verify radvd installation
radvd -v
```

### 3.3 Common Tools on CN and MN

Run on **CN** and **MN**:

```bash
sudo dnf install -y net-tools iproute wget
```

> **Important:** After installing all software packages, power off the VMs and configure the VirtualBox network adapters according to the [Network Topology Table](#virtual-machine-network-adapters-setup) (disconnecting NAT).

---

## 4. Node Configurations

### Home Agent (HA)

#### 1. Network Interface Setup (`nmcli`)
```bash
# List network connections and obtain connection UUIDs
nmcli c

# Edit and save persistent settings if required
nmcli c edit <UUID>
# Inside interactive prompt: save persistent -> quit

# Configure Adapter 2 (enp0s8) - Connecting to Network-2 (2001:db8:2::/64)
sudo nmcli con modify "Wired connection 1" ipv6.addresses "2001:db8:2::1/64" ipv6.routes "2001:db8:3::/64 2001:db8:2::2" ipv6.method manual
sudo nmcli con up "Wired connection 1"

# Configure Adapter 1 (enp0s3) - Home Link Network-1 (2001:db8:1::/64)
sudo nmcli con modify "enp0s3" ipv6.addresses "2001:db8:1::1/64" ipv6.method manual
sudo nmcli con up "enp0s3"

# Verify IP addresses
ip -6 a
```

#### 2. Configure Router Advertisement Daemon (`radvd`)
Edit `/etc/radvd.conf`:
```bash
sudo gedit /etc/radvd.conf
```

Add the following configuration:
```nginx
interface enp0s3
{
    AdvSendAdvert on;
    AdvIntervalOpt off;
    AdvHomeAgentFlag on;
    MaxRtrAdvInterval 3;
    MinRtrAdvInterval 1;
    HomeAgentLifetime 10000;
    HomeAgentPreference 20;
    AdvHomeAgentInfo on;
    prefix 2001:db8:1::1/64
    {
        AdvRouterAddr on;
        AdvOnLink on;
        AdvAutonomous on;
        AdvPreferredLifetime 10000;
        AdvValidLifetime 12000;
    };
};
```

Enable and start `radvd`:
```bash
sudo systemctl enable --now radvd
sudo systemctl status radvd
```

#### 3. Kernel Parameters
Create `/etc/sysctl.d/zz-test.conf`:
```bash
sudo gedit /etc/sysctl.d/zz-test.conf
```

Insert:
```ini
net.ipv6.conf.all.forwarding = 1
net.ipv6.conf.all.autoconf = 0
net.ipv6.conf.all.accept_ra = 0
net.ipv6.conf.all.accept_redirects = 0
```

Apply parameters:
```bash
sudo sysctl --system
```

#### 4. Configure MIPv6 Daemon (`/etc/mip6d.conf`)
```bash
sudo gedit /etc/mip6d.conf
```

Insert:
```nginx
NodeConfig HA;
DebugLevel 10;
DoRouteOptimizationCN enabled;
BindingAclPolicy 2001:db8:1::100 allow;
Interface "enp0s3";
UseMnHaIPsec disabled;
DefaultBindingAclPolicy deny;
```

#### 5. Disable Firewall
```bash
sudo systemctl disable --now firewalld
```

---

### Foreign Router (Router)

#### 1. Network Interface Setup (`nmcli`)
```bash
# Configure Adapter 2 (enp0s8) - Connecting to Network-3 (2001:db8:3::/64)
sudo nmcli con modify "Wired connection 1" ipv6.addresses "2001:db8:3::1/64" ipv6.method manual
sudo nmcli con up "Wired connection 1"

# Configure Adapter 1 (enp0s3) - Connecting to Network-2 (2001:db8:2::/64)
sudo nmcli con modify "enp0s3" ipv6.addresses "2001:db8:2::2/64" ipv6.routes "2001:db8:1::/64 2001:db8:2::1" ipv6.method manual
sudo nmcli con up "enp0s3"
```

#### 2. Configure `radvd`
Edit `/etc/radvd.conf`:
```bash
sudo gedit /etc/radvd.conf
```

Insert:
```nginx
interface enp0s8
{
    AdvSendAdvert on;
    AdvIntervalOpt on;
    MinRtrAdvInterval 1;
    MaxRtrAdvInterval 3;
    AdvHomeAgentFlag off;
    prefix 2001:db8:3::1/64
    {
        AdvOnLink on;
        AdvAutonomous on;
        AdvRouterAddr on;
    };
};
```

Enable and start `radvd`:
```bash
sudo systemctl enable --now radvd
sudo systemctl status radvd
```

#### 3. Kernel Parameters
Create `/etc/sysctl.d/zz-test.conf`:
```bash
sudo gedit /etc/sysctl.d/zz-test.conf
```

Insert:
```ini
net.ipv6.conf.all.forwarding = 1
net.ipv6.conf.all.autoconf = 0
net.ipv6.conf.all.accept_ra = 0
net.ipv6.conf.all.accept_redirects = 0
```

Apply parameters:
```bash
sudo sysctl --system
```

#### 4. Disable Firewall
```bash
sudo systemctl disable --now firewalld
```

---

### Correspondent Node (CN)

#### 1. Network Interface & Route Setup
```bash
# Configure interface enp0s3 on Network-3
sudo nmcli con modify "enp0s3" ipv6.addresses "2001:db8:3::2/64" ipv6.routes "2001:db8:1::/64 2001:db8:3::1" ipv6.method manual
sudo nmcli con up "enp0s3"

# Optional: Set default gateway if routing fails
sudo nmcli con modify "enp0s3" ipv6.gateway "2001:db8:3::1"
sudo nmcli con up "enp0s3"
```

#### 2. Configure MIPv6 Daemon (`/etc/mip6d.conf`)
```bash
sudo gedit /etc/mip6d.conf
```

Insert:
```nginx
NodeConfig CN;
DebugLevel 10;
DoRouteOptimizationCN enabled;
```

#### 3. Disable Firewall
```bash
sudo systemctl disable --now firewalld
```

---

### Mobile Node (MN)

#### 1. Configure MIPv6 Daemon (`/etc/mip6d.conf`)
```bash
sudo gedit /etc/mip6d.conf
```

Insert:
```nginx
NodeConfig MN;
DebugLevel 10;
DoRouteOptimizationMN enabled;
UseMnHaIPsec disabled;
MnHomeLink "enp0s3" {
    HomeAgentAddress 2001:db8:1::1;
    HomeAddress 2001:db8:1::100/64;
}
Interface "enp0s3";
```

#### 2. Kernel Parameters
Create `/etc/sysctl.d/zz-test.conf`:
```bash
sudo gedit /etc/sysctl.d/zz-test.conf
```

Insert:
```ini
net.ipv6.conf.all.forwarding = 0
net.ipv6.conf.all.autoconf = 1
net.ipv6.conf.all.accept_ra = 1
net.ipv6.conf.enp0s3.accept_ra = 1
```

Apply parameters:
```bash
sudo sysctl --system
```

#### 3. Disable Firewall
```bash
sudo systemctl disable --now firewalld
```

---

## 5. Baseline Connectivity Verification

Before initiating MIPv6 mobility daemons, verify basic IPv6 routing across all static nodes:

### From Home Agent (HA):
```bash
# Ping Router's Network-2 interface (Direct connection)
ping6 -c 3 2001:db8:2::2

# Ping Router's Network-3 interface (Across Router)
ping6 -c 3 2001:db8:3::1
```

### From Foreign Router:
```bash
# Ping HA's Network-2 interface (Direct connection)
ping6 -c 3 2001:db8:2::1

# Ping HA's Home Network interface
ping6 -c 3 2001:db8:1::1
```

### From Correspondent Node (CN):
```bash
# Ping Router Network-3 interface (Direct link)
ping6 -c 3 2001:db8:3::1

# Ping Router Network-2 interface
ping6 -c 3 2001:db8:2::2

# Ping Home Agent on Home Network
ping6 -c 3 2001:db8:1::1
```

---

## 6. Executing MIPv6 & Handoff Demonstration

### Step 1: Start MIPv6 Daemons
Open terminal windows on **HA**, **CN**, and **MN** and execute `mip6d` in debug/foreground mode:

```bash
# On HA, CN, and MN:
sudo mip6d -c /etc/mip6d.conf
```

### Step 2: Initiate Ping Session from Mobile Node
From the **MN** terminal, start an ongoing ping to the Correspondent Node (CN):
```bash
ping6 2001:db8:3::2
```

### Step 3: Simulate Handoff / Network Movement
1. While the `ping6` command is running, open VirtualBox settings for **Mobile-Node (MN)**.
2. Go to **Network** -> **Adapter 1**.
3. Change **Attached to: Internal Network** name from `Network-1` to `Network-3` (simulating movement from Home Link to Foreign Link).
4. Click **OK**.

### Step 4: Expected Result
* The `mip6d` daemon on **MN** receives Router Advertisements from the Foreign Router (`2001:db8:3::1/64`).
* MN auto-configures a **Care-of Address (CoA)** (e.g., `2001:db8:3::xxxx/64`).
* MN sends a **Binding Update (BU)** to HA (`2001:db8:1::1`).
* HA responds with a **Binding Acknowledgment (BA)** and sets up an IPv6-in-IPv6 tunnel to the MN's Care-of Address.
* The ICMPv6 ping sequence between MN (`2001:db8:1::100`) and CN (`2001:db8:3::2`) **resumes automatically** with minimal packet loss.

---

## 7. Cross-Check & Troubleshooting Notes

### 🔍 Verification & Audit Checklist
| Item | Verification / Recommendation | Status |
| :--- | :--- | :---: |
| **SLAAC on MN** | MN must accept Router Advertisements to form a Care-of Address (CoA) on `Network-3`. Verified `net.ipv6.conf.all.accept_ra = 1` and `autoconf = 1`. | ✅ Verified |
| **Routing Tables** | Static routes configured between HA (`2001:db8:3::/64 via 2001:db8:2::2`) and Router (`2001:db8:1::/64 via 2001:db8:2::1`). | ✅ Verified |
| **Firewall (firewalld)** | MIPv6 encapsulates packets using IPv6 Tunneling (IP protocol 41) and Mobility headers (protocol 135). Disabling `firewalld` prevents silent drops. | ✅ Verified |
| **CN `mip6d` Process** | Though optional for pure bi-directional tunneling (HA anchor), running `mip6d` on CN allows **Route Optimization (RO)** directly between MN and CN. | ✅ Recommended |
| **IPsec Security** | Set `UseMnHaIPsec disabled;` on both HA and MN to bypass IKE/IPsec setup for simplified testing environments. | ✅ Verified |
