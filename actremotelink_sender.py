#!/usr/bin/env python3
import argparse
import socket
import subprocess
import sys
import time
from pathlib import Path


def dec_to_hex_id(dec_id):
    """Converts a decimal string account id to a 64-bit hex id."""
    try:
        int_id = int(dec_id)
        return f"0x{int_id:016x}"
    except (ValueError, TypeError):
        return None


def run_make():
    print("[+] Build: make")
    subprocess.run(["make"], check=True)


def send_elf_to_loader(host, port, elf_path, max_seconds=8, wait_for_pairing=False):
    elf = Path(elf_path)

    if not elf.exists():
        raise SystemExit(f"[-] ELF not found: {elf}")

    data = elf.read_bytes()

    print(f"[+] Enviando ELF: {elf}")
    print(f"[+] Destino: {host}:{port}")
    print(f"[+] Tamanho: {len(data)} bytes")

    with socket.create_connection((host, port), timeout=10) as s:
        s.sendall(data)

        try:
            s.shutdown(socket.SHUT_WR)
        except OSError:
            pass

        if wait_for_pairing:
            timeout_total = max_seconds
        else:
            timeout_total = max_seconds

        s.settimeout(1.0)
        started = time.time()
        collected = ""

        while time.time() - started < timeout_total:
            try:
                chunk = s.recv(4096)

                if not chunk:
                    break

                text = chunk.decode(errors="replace")
                collected += text

                sys.stdout.write(text)
                sys.stdout.flush()

                if wait_for_pairing:
                    if "PAIRING SUCCESS" in collected:
                        break
                    if "PAIRING FAILED" in collected:
                        break
                    if "Pairing expired" in collected:
                        break

            except socket.timeout:
                if not wait_for_pairing:
                    break
                continue
            except ConnectionResetError:
                print("\n[-] Connection closed by the PS5.")
                break

    return collected


def agent_command(host, port, command, timeout=5):
    with socket.create_connection((host, port), timeout=timeout) as s:
        s.sendall((command.strip() + "\n").encode())
        try:
            s.shutdown(socket.SHUT_WR)
        except OSError:
            pass

        s.settimeout(timeout)
        chunks = []

        while True:
            try:
                chunk = s.recv(4096)
                if not chunk:
                    break

                chunks.append(chunk)

                text = b"".join(chunks).decode(errors="replace")
                if "\nEND\n" in text or text.endswith("END\n"):
                    break

            except socket.timeout:
                break

    return b"".join(chunks).decode(errors="replace")


def is_agent_online(host, port):
    try:
        resp = agent_command(host, port, "HELP", timeout=2)
        return "END" in resp or "OK" in resp
    except Exception:
        return False


def ensure_agent(host, loader_port, agent_port, agent_elf, wait):
    if is_agent_online(host, agent_port):
        return

    print("[+] Agent is not online. Sending Agent ELF...")
    send_elf_to_loader(
        host,
        loader_port,
        agent_elf,
        max_seconds=4,
        wait_for_pairing=False
    )

    time.sleep(wait)

    if not is_agent_online(host, agent_port):
        raise SystemExit("[-] Agent did not respond after sending.")


def print_response(text):
    if text:
        print(text, end="" if text.endswith("\n") else "\n")
    else:
        print("[-] No response.")


def main():
    parser = argparse.ArgumentParser(
        description="ActRemoteLink sender"
    )

    parser.add_argument("--host", default="ps5")
    parser.add_argument("--loader-port", type=int, default=9021)
    parser.add_argument("--agent-port", type=int, default=31337)
    parser.add_argument("--elf", default="actremotelink_agent.elf")
    parser.add_argument("--pin-elf", default="actremotelink_pin_notify.elf")
    parser.add_argument("--wait", type=float, default=2.0)
    parser.add_argument("--build", action="store_true")
    parser.add_argument("--no-send", action="store_true")

    sub = parser.add_subparsers(dest="cmd", required=True)

    sub.add_parser("start")
    sub.add_parser("users")
    sub.add_parser("current")
    sub.add_parser("quit")
    sub.add_parser("help")
    sub.add_parser("prepare-pin")
    sub.add_parser("paired")
    sub.add_parser("compare")

    p_b64 = sub.add_parser("b64")
    p_b64.add_argument("--id", required=True)

    p_set = sub.add_parser("set-id")
    p_set.add_argument("--slot", required=True)
    p_set.add_argument("--id", required=True)

    sub.add_parser("pin")

    p_get_key = sub.add_parser("get-key")
    p_get_key.add_argument("--type", choices=["int", "str", "bin"], required=True)
    p_get_key.add_argument("--id", required=True)

    p_set_key = sub.add_parser("set-key")
    p_set_key.add_argument("--type", choices=["int", "str", "bin"], required=True)
    p_set_key.add_argument("--id", required=True)
    p_set_key.add_argument("--val", required=True)

    p_fake = sub.add_parser("fake-signin")
    p_fake.add_argument("--slot", required=True)

    args = parser.parse_args()

    if args.build:
        run_make()

    if args.cmd == "pin":
        send_elf_to_loader(
            args.host,
            args.loader_port,
            args.pin_elf,
            max_seconds=330,
            wait_for_pairing=True
        )
        return

    if args.cmd == "start":
        if is_agent_online(args.host, args.agent_port):
            print("[+] Agent is already online.")
            return

        send_elf_to_loader(
            args.host,
            args.loader_port,
            args.elf,
            max_seconds=4,
            wait_for_pairing=False
        )

        time.sleep(args.wait)

        if is_agent_online(args.host, args.agent_port):
            print("[+] Agent online.")
        else:
            print("[-] Agent sent, but it did not respond.")
        return

    if not args.no_send:
        ensure_agent(
            args.host,
            args.loader_port,
            args.agent_port,
            args.elf,
            args.wait
        )

    if args.cmd == "users":
        print_response(agent_command(args.host, args.agent_port, "LIST"))

    elif args.cmd == "current":
        print_response(agent_command(args.host, args.agent_port, "CURRENT"))

    elif args.cmd == "quit":
        print_response(agent_command(args.host, args.agent_port, "QUIT"))

    elif args.cmd == "help":
        print_response(agent_command(args.host, args.agent_port, "HELP"))

    elif args.cmd == "prepare-pin":
        print_response(agent_command(args.host, args.agent_port, "PREPARE_PIN"))

    elif args.cmd == "paired":
        print_response(agent_command(args.host, args.agent_port, "PAIRED"))

    elif args.cmd == "compare":
        print_response(agent_command(args.host, args.agent_port, "COMPARE"))

    elif args.cmd == "b64":
        hex_id = dec_to_hex_id(args.id) or args.id
        print_response(agent_command(args.host, args.agent_port, f"B64 {hex_id}"))

    elif args.cmd == "set-id":
        try:
            slot_num = int(args.slot)
        except ValueError:
            slot_num = -1

        if slot_num == 1:
            try:
                user_list = agent_command(args.host, args.agent_port, "LIST")
                is_user1 = False
                for line in user_list.splitlines():
                    if "slot=1 " in line and ('name="User1"' in line or 'name="user1"' in line):
                        is_user1 = True
                        break
                if is_user1:
                    print("[-] Error: You cannot modify User1 (slot 1) to prevent save game loss.")
                    print("    Please create a new offline user on your PS5 and use slot 2 (or higher).")
                    sys.exit(1)
            except Exception as e:
                print(f"[-] Error verifying user list: {e}")
                sys.exit(1)

        hex_id = dec_to_hex_id(args.id) or args.id
        print_response(
            agent_command(
                args.host,
                args.agent_port,
                f"SETID {args.slot} {hex_id}"
            )
        )

    elif args.cmd == "get-key":
        print_response(
            agent_command(
                args.host,
                args.agent_port,
                f"GETKEY {args.type} {args.id}"
            )
        )

    elif args.cmd == "set-key":
        print_response(
            agent_command(
                args.host,
                args.agent_port,
                f"SETKEY {args.type} {args.id} {args.val}"
            )
        )

    elif args.cmd == "fake-signin":
        try:
            slot_num = int(args.slot)
        except ValueError:
            print("[-] Error: --slot must be a number.")
            sys.exit(1)

        if slot_num < 2:
            print("[-] Error: Cannot fake-signin on slot 1 (protection active).")
            print("    Use slot 2 or higher.")
            sys.exit(1)

        print_response(
            agent_command(
                args.host,
                args.agent_port,
                f"FAKESIGNIN {args.slot}",
                timeout=15
            )
        )


if __name__ == "__main__":
    main()
