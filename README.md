## ActRemoteLink

**ActRemoteLink** is a set of payloads and utilities for PS5 jailbreak workflows focused on offline activation of a local account and Remote Play / Chiaki pairing without PSN access.

> [!CAUTION]
> <span style="color: #ff3333; font-weight: bold;">⚠️ CRITICAL WARNING:</span> Modifying the local `account_id` can disassociate or corrupt your local save games. **Always back up all your save games, licenses, and any system backups you deem necessary before proceeding!**
> 
> **Important Guidelines & Safety Measures:**
> * **Slot 1 (User1) is a legitimate PSN-activated account:** If your jailbreak relies on a system restore backup (e.g., Gezine), Slot 1 is already activated/associated and is always enabled for Remote Play. Changing the `account_id` or running activation on Slot 1 is unnecessary and should be avoided to prevent save game loss.
> * **Activating other accounts (Slot 2 or higher):** To use your original PSN account or another custom account offline, **create a new local user** (which will occupy Slot 2 or higher). You will then run the fake activation (`set-id`) and the integrated NP fake sign-in (`fake-signin`) exclusively on this new user account.
> * **Mandatory Foreground User State:** When executing pairing or establishing a Remote Play connection, the offline user you want to stream to (e.g., Slot 2) **must be the active foreground user logged in on the PS5 screen**. This is a requirement for Remote Play/Chiaki to access the session.

---

### Case Study: Existing Jailbroken PS5 — Multiple Situations

This section explains common real-world scenarios where users may already have an existing local account, restored backup, or save data before using this project.

The goal is to avoid unnecessary account modification, especially on accounts that already have save games or previous PSN association.

---

#### Situation 1: Previously PSN-Activated or Associated Account

If the account you want to use for Remote Play was already legitimately PSN-activated or associated at least once on that PS5, you usually do **not** need to modify its `account_id`.

This is a separate scenario from the Gezine backup case.

For users who already have a legitimately activated or previously associated account on the console, the recommended approach is to first test Remote Play without making any account modifications.

> [!TIP]
> **Try the normal path first before modifying anything:**
> 1. Log into the user you want to stream from.
> 2. Enable Remote Play in the PS5 settings.
> 3. Register the console in Chiaki or PS Remote Play.
> 4. Make sure that this user is the active foreground user on the PS5 screen.
>
> If Remote Play works using this method, you do not need to use `set-id` or change the account identifier for that user.

---

#### Situation 2: Gezine Backup / User1 / Slot 1

A common scenario is the following:

You jailbroke your PS5 months ago, restored a system backup such as Gezine's backup, and continued using **User1 / Slot 1** as your normal account for playing games, creating save data, and using the console day to day.

Later, you decide that you want to use Remote Play from a PC, either through **Chiaki** or the official **PS Remote Play** app.

In this case, the most important thing to understand is:

> [!NOTE]
> If you restored Gezine's backup and continued using User1 / Slot 1 as your main playing account, that user may already be associated in a way that allows Remote Play to work after simply enabling Remote Play.
>
> So, before modifying anything, first try to enable Remote Play normally on User1 / Slot 1.

If Remote Play still does not work, you may try the project's **fake-signin** option on Slot 1 (see [Step 5: NP Fake Sign-in](#5-np-fake-sign-in-integrated-offline-psn-sign-in) in the Quick Start section).

> [!WARNING]
> **Do not change the `account_id` of Slot 1 / User1 from Gezine's backup.**
>
> The dangerous action is modifying the `account_id` of the existing User1 from the restored backup.
>
> Doing that can create a conflict between the local user, the account association, and the save data already tied to that user. The result may be that your games or saves stop working as expected.

---

#### Recommended Safe Approach to Keep Your Save Games and Settings

If you want to use this project to activate an offline account, the safer approach is:

> [!IMPORTANT]
> 1. Leave **User1 / Slot 1** untouched.
> 2. Create a new local user, which will become **Slot 2 or higher**.
> 3. Run `set-id` and `fake-signin` only on this new user.
> 4. Enable Remote Play for the new user.
> 5. Make sure the new user is the active foreground user before pairing or connecting.
> 6. After confirming everything works, you may use a save manager such as **Garlic Save Manager** to migrate or resign saves from User1 to the new user, if needed.

---


This project was updated and consolidated using concepts, references, and workflow improvements from **[LinkDev](https://github.com/ps5-payload-dev/linkdev)**, **[OffAct](https://github.com/ps5-payload-dev/offact)**, and **[ps5-remoteplay-get-pin](https://github.com/idlesauce/ps5-remoteplay-get-pin)**, while also taking advantage of newer versions of the **[ps5-payload-dev SDK](https://github.com/ps5-payload-dev/sdk/releases)**.

Recent SDK updates, especially versions **v0.38**, **v0.39**, and **v0.40**, added important compatibility improvements, including CRT support for firmware **13.00** and **13.20**, as well as kernel offset support for **11.xx** and **12.xx** firmware versions. These updates helped modernize the payload structure and improve firmware compatibility.

The current release uses two main components:

* `actremotelink_agent.elf`: manages local users, `account_id`, and Base64 handling.
* `actremotelink_pin_notify.elf`: generates the real Remote Play PIN via `libSceRemoteplay` and shows it through a PS5 notification.

The project does not depend on `ptrace`, does not use SDL, does not open its own screen, and does not require manually navigating the PS5 system menu to generate the PIN.

---

## Features

* List local PS5 users.
* Identify the current user.
* Read the current user's `account_id`.
* Change `account_id` offline.
* Generate the correct Base64 for `account_id`.
* Enable Remote Play.
* Generate the real Remote Play PIN.
* Display the PIN through a PS5 notification.
* Monitor pairing.
* Exit automatically after success.

---

## Project Layout

```text
ActRemoteLink/
├── Makefile
├── actremotelink_agent.c
├── actremotelink_agent.elf
├── actremotelink_pin_notify.c
├── actremotelink_pin_notify.elf
└── actremotelink_sender.py
```

---

## Requirements

* PS5 with jailbreak.
* Payload loader active on port `9021`.
* PS5 reachable via the hostname `ps5` or an IP address.
* PS5 Payload SDK installed.
* Linux/Kali environment.

## Build

Clone the repository and build the project:

```bash
# Clone the repository
git clone https://github.com/francoataffarel/ActRemoteLink.git
cd ActRemoteLink

# Set the SDK path (adjust to your local installation)
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk

# Compile
make clean
make
```

Generated files:

```text
actremotelink_agent.elf
actremotelink_pin_notify.elf
```


---

## Quick Start

### 1. Start the Agent

```bash
python3 actremotelink_sender.py --host ps5_IP start
```

Expected result:

```text
[+] Agent online.
```

---

### 2. View the Current User

```bash
python3 actremotelink_sender.py --host ps5_IP --no-send current
```

Example output:

```text
OK CURRENT
USER slot=1 current=1 user_id=500000001 name="User1" type="np" flags=0x0006 id=0x0123456789abcdef b64=789q2UZ0IwA=
END
```

---

### 3. List Users

```bash
python3 actremotelink_sender.py --host ps5_IP --no-send users
```

Example output:

```text
OK LIST
USER slot=1 current=0 user_id=500000001 name="User1" type="np" flags=0x1002 id=0x0123456789abcdef b64=789q2UZ0IwA=
USER slot=2 current=1 user_id=500000002 name="MyUser" type="" flags=0x10000 id=0x0000000000000000 b64=AAAAAAAAAAA=
END
```

---

### 4. Change account_id (Fake Account Activation)

Use this to activate a local user account offline by assigning a custom `account_id`. The payload will also automatically set the account type to `np` and the account flags to `4098` (`0x1002`) in the registry, achieving full activation.
To use your own `account_id` for Remote Play, you need to find its decimal value from your PlayStation Network account and provide it to the sender script. The script will automatically convert it to the required hexadecimal format.

### Step-by-step Guide

1.  **Log in to your PSN account:**
    *   Go to [https://www.playstation.com/en-us/](https://www.playstation.com/en-us/) and log in.

2.  **Navigate to Account Management:**
    *   After logging in, go to **Account Management**, then click on your **Profile**.

3.  **Find the `accountId`:**
    *   Open your browser's developer tools (e.g., `Ctrl+Shift+C` in Chrome).
    *   Go to the **Network** tab.
    *   Look for a `GET` request to an address similar to:
        `https://web.np.playstation.com/api/basicProfile/v1/profile/users/me`
    *   Check the response for this request. It will be a JSON file containing your profile information.
    *   Find the `accountId` attribute and copy its numeric value. It will look like this:
        `"accountId":"PLEASECHANGEME_DEC"`

4.  **Use the `accountId` in the sender:**
    *   Use the copied numeric value directly in the `set-id` or `b64` command. The script will handle the conversion.

    **Example:**
    ```bash
    # Set the account_id, type (np), and flags (0x1002) using the decimal value on slot 2
    python3 actremotelink_sender.py --host ps5_IP --no-send set-id --slot 2 --id PLEASECHANGEME_DEC

    # Calculate Base64 from the decimal value
    python3 actremotelink_sender.py --host ps5_IP --no-send b64 --id PLEASECHANGEME_DEC
    ```

The script will convert `PLEASECHANGEME_DEC` to `0xPLEASECHANGEME_HEX` internally before sending it to the agent.

**After changing `account_id`, it is recommended to restart the PS5.**

---

### 5. NP Fake Sign-in (Integrated Offline PSN Sign-in)

For Remote Play connections to succeed on accounts created offline (e.g., Slot 2), the account must be signed in to the PlayStation Network (NP) state. The agent includes an integrated version of **np-fake-signin** which writes the necessary NP data files (`auth.dat`, `config.dat`) and populates the registry login keys.

Before running the command, ensure that:
* The user you want to sign-in is the **active foreground user** on the PS5 screen.
* The account has been activated (using the `set-id` command in Step 4).

Run the fake sign-in command:
```bash
python3 actremotelink_sender.py --host ps5_IP --no-send fake-signin --slot 2
```

Expected output:
```text
OK FAKESIGNIN
slot=2
user_id=0x1dcafe01
username="MyUser"
account_id=0xfedcba9876543210
country="us"
lang="en"
locale="en-US"
write_auth_cfg=0/0
reboot_recommended=1
END
```

**After performing fake sign-in, it is recommended to restart the PS5.**

---

### 6. Generate the Remote Play PIN by notification

```bash
python3 actremotelink_sender.py --host ps5_IP pin
```

Or with automatic build before sending:

```bash
python3 actremotelink_sender.py --host ps5_IP --build pin
```

Expected terminal output:
```text
OK PIN
slot=2
pin=2836 0549
pin_raw=28360549
account_id_hex=0x8f1e2d3c4b5a6978
account_id_base64=eGlaSzwtHo8=
timeout=300
END
PAIRING SUCCESS
```

The PS5 will show a notification similar to:

```text
ActRemoteLink PIN
PIN: 2836 0549
ID: eGlaSzwtHo8=
Time: 300 s
```

---

### 7. Stop the Agent

When you are done with the process, you can stop the agent running on the PS5:

```bash
python3 actremotelink_sender.py --host ps5_IP --no-send quit
```

---

## Pairing with Chiaki / Remote Play

Use this in the client:

```text
PIN: the PIN shown in the notification
Account ID: the Base64 shown in the notification
```

Example:

```text
PIN: 2836 0549
Account ID: eGlaSzwtHo8=
```

When pairing completes, the payload prints:

```text
PAIRING SUCCESS
```

And the PS5 shows:

```text
ActRemoteLink
Pairing completed successfully
```

After that, the payload exits automatically.

---

## Full Workflow

```text
1. Build the project.
2. Start the Agent on the PS5.
3. Check the current user.
4. Adjust account_id if needed (set-id).
5. Ensure the target user is active in the foreground and perform NP fake sign-in (fake-signin) to generate the PSN files.
6. Restart the PS5 to apply.
7. Log into the target user in the foreground, go to Settings > System > Remote Play, and ensure "Enable Remote Play" is toggled ON.
8. Run the pin command through the sender.
9. Read the PIN and Account ID from the notification.
10. Pair in Chiaki / Remote Play.
11. The payload detects success and exits.
```

---

## Daily Workflow

Once `account_id` is already correct, the normal usage is just:

```bash
cd /path/to/ActRemoteLink
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk

python3 actremotelink_sender.py --host ps5_IP pin
```

---

## Main Commands

```bash
# Build
make clean
make

# Start Agent
python3 actremotelink_sender.py --host ps5_IP start

# View current user
python3 actremotelink_sender.py --host ps5_IP --no-send current

# List users
python3 actremotelink_sender.py --host ps5_IP --no-send users

# Calculate Base64 for account_id (accepts decimal or hex)
python3 actremotelink_sender.py --host ps5_IP --no-send b64 --id PLEASECHANGEME_DEC

# Change account_id, type (np), and flags (0x1002) on slot 2 (accepts decimal or hex)
python3 actremotelink_sender.py --host ps5_IP --no-send set-id --slot 2 --id PLEASECHANGEME_DEC

# Perform NP fake sign-in (runs offline PSN sign-in using templates)
python3 actremotelink_sender.py --host ps5_IP --no-send fake-signin --slot 2

# Generate the PIN with automatic build
python3 actremotelink_sender.py --host ps5_IP --build pin

# Stop/Shutdown Agent
python3 actremotelink_sender.py --host ps5_IP --no-send quit

# Query a registry key by type and ID (int, str, bin)
python3 actremotelink_sender.py --host ps5_IP --no-send get-key --type str --id 125874188

# Set a registry key by type, ID, and value (int, str, bin)
python3 actremotelink_sender.py --host ps5_IP --no-send set-key --type str --id 125874188 --val "MyOnlineID"
```

---

## Module Differences

### actremotelink_agent.elf

Responsible for persistent account operations:

```text
LIST
CURRENT
B64
SETID
PREPARE_PIN
PAIRED
COMPARE
GETKEY
SETKEY
FAKESIGNIN
QUIT
HELP
```

### actremotelink_pin_notify.elf

Temporary payload responsible for:

```text
- initialize Remote Play;
- generate the real PIN;
- show a notification;
- monitor pairing;
- exit after success or timeout.
```

---

## Troubleshooting Reconnection Issues (Error 0x80108b12)

If pairing succeeds but trying to connect in Chiaki fails immediately with `0x80108b12` (HTTP 403 Forbidden), it means the target user account on the PS5 lacks the required PlayStation Network (NP) files and registry sign-in state (common for accounts created entirely offline).

To fix this:
1. Ensure you have run the integrated fake sign-in command (`python3 actremotelink_sender.py --host ps5_IP --no-send fake-signin --slot 2`) while having the target user account active in the foreground on the PS5.
2. Reboot the PS5 to apply.
3. Log into the target user account, go to **Settings > System > Remote Play**, and ensure the **"Enable Remote Play"** toggle is turned **ON** for the account (on offline accounts, this toggle may require manual enablement after fake sign-in, even though the menu is normally grayed out for unlinked accounts).
4. Launch Chiaki and connect!

---

## Important Notes

*   **Reboot Required:** After changing the `account_id` or performing fake account activation (`set-id`), you **must restart/reboot the PS5** for the system services to reload the new database values.
*   **Settings Menu is Grayed Out:** On jailbroken consoles, the "Enable Remote Play" toggle in the PS5 system settings menu may remain grayed out (disabled) because the console is offline and cannot connect to PSN. **This is normal and does not prevent Remote Play from working.** The background pairing service will still accept connections and complete pairing successfully.
*   **Real PIN Generation:** The displayed PIN is **not fake**. It is generated by the real `libSceRemoteplay` function:

    ```c
    sceRemoteplayGeneratePinCode(&pin);
    ```

*   **Base64 Conversion:** The `account_id` Base64 is generated from the raw 8 bytes in little-endian order, not from the hexadecimal string.

    Example:
    ```text
    account_id_hex=0x8f1e2d3c4b5a6978
    account_id_base64=eGlaSzwtHo8=
    ```

---

## Current Status

Recommended release:

```text
ActRemoteLink
```

Status:

```text
Agent functional.
account_id change functional.
Base64 validated.
Real Remote Play PIN functional.
PS5 notification functional.
Pairing confirmed.
No ptrace.
No SDL.
No manual menu.
```

---

## Recommended Checkpoint

After validating everything:

```bash
cd ..

cp -a ActRemoteLink ActRemoteLink_ok
tar -czf ActRemoteLink_ok.tar.gz ActRemoteLink
```

---

## License

ActRemoteLink is licensed under GPL-3.0-or-later.

This project uses concepts and compatibility references from PS5 homebrew projects such as OffAct, LinkDev, ps5-remoteplay-get-pin, and np-fake-signin. Code derived from GPL-licensed projects remains under GPL-3.0-or-later.

No Sony proprietary code, firmware, keys, decrypted modules, or copyrighted PlayStation binaries are included in this repository.
---

## Credits and Thank you guys

- **[earthonion](https://git.etawen.dev/earthonion) — [np-fake-signin](https://git.etawen.dev/earthonion/np-fake-signin)** — fake PSN sign-in tool for PS4/PS5 that resolves Remote Play connection rejections on offline users.
- **[ps5-payload-dev](https://github.com/ps5-payload-dev) — [OffAct](https://github.com/ps5-payload-dev/offact)** — PS5 offline account activation homebrew.
- **[ps5-payload-dev](https://github.com/ps5-payload-dev) — [LinkDev](https://github.com/ps5-payload-dev/linkdev)** — local Remote Play device pairing for jailbroken PS5s without PSN access.
- **[idlesauce](https://github.com/idlesauce) — [ps5-remoteplay-get-pin](https://github.com/idlesauce/ps5-remoteplay-get-pin)** — Remote Play pairing PIN generation for offline activated accounts.
- **[Macedo](https://x.com/Macedo95766776) / [ps4macedo](https://github.com/ps4macedo)** — PS4/PS5 community tooling, ports, testing, and shared resources around Y2JB/P2JB workflows.
- **[john-tornblom](https://github.com/john-tornblom)** — elfldr and ps5-payload-sdk (used for compilation).
- **[ufm42](https://github.com/ufm42)** — [kexp](https://github.com/ufm42/kexp), the PS5 post-jailbreak all-in-one shellcode used by the payloads.
- **[thestr4ng3r](https://sr.ht/~thestr4ng3r/) — [Chiaki](https://sr.ht/~thestr4ng3r/chiaki/)** — free and open-source Remote Play client that enabled PS4/PS5 streaming workflows and inspired Chiaki-compatible pairing support.
- **Claude (Anthropic)** - debugging assistance.
- **Chatgpt** — debugging assistance.
- **Gemini** — debugging assistance. 
