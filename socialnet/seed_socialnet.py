#!/usr/bin/env python3
import argparse
import json
import random
import string
import time

try:
    import requests
except Exception as e:
    print("This script requires the 'requests' library. Install with: pip install requests")
    raise


def get_request(session: requests.Session, base_url: str, path: str, params: dict, timeout: float) -> requests.Response:
    url = base_url.rstrip("/") + path
    return session.get(url, params=params, timeout=timeout)


def get_timeline(session: requests.Session, base_url: str, path: str, params: dict, timeout: float) -> requests.Response:
    url = base_url.rstrip("/") + path
    return session.get(url, params=params, timeout=timeout)


def random_text(min_len: int = 10, max_len: int = 80) -> str:
    length = random.randint(min_len, max_len)
    alphabet = string.ascii_letters + string.digits + " "
    return ''.join(random.choice(alphabet) for _ in range(length))


def main():
    parser = argparse.ArgumentParser(description="Seed the socialnet frontend with users and posts")
    parser.add_argument("--base-url", default="http://localhost:50060", help="Frontend base URL")
    parser.add_argument("--num-users", type=int, default=200, help="Number of users to register")
    parser.add_argument("--num-posts", type=int, default=1000, help="Number of posts to compose")
    parser.add_argument("--num-follows", type=int, default=2000, help="Number of follow relationships to create")
    parser.add_argument("--username-prefix", default="seeduser", help="Prefix for generated usernames")
    parser.add_argument("--timeout", type=float, default=10.0, help="HTTP timeout seconds")
    parser.add_argument("--retries", type=int, default=3, help="Retries per request on timeout/error")
    parser.add_argument("--mentions", action="store_true", help="Include @mentions and URLs in some posts")
    args = parser.parse_args()

    base_url = args.base_url
    usernames = [f"{args.username_prefix}{i+1}" for i in range(args.num_users)]

    print(f"Registering {len(usernames)} users against {base_url} ...")
    with requests.Session() as session:
        for name in usernames:
            attempt = 0
            while True:
                attempt += 1
                try:
                    r = get_request(session, base_url, "/register", {"username": name}, args.timeout)
                    print(f"register {name}: status={r.status_code}, bytes={len(r.content)}")
                    break
                except Exception as e:
                    if attempt >= args.retries:
                        print(f"register {name}: error after {attempt} attempts: {e}")
                        break
                    backoff = 0.2 * attempt
                    print(f"register {name}: retrying in {backoff:.1f}s due to: {e}")
                    time.sleep(backoff)
            time.sleep(0.02)

    # Create follow relationships
    print(f"\nCreating {args.num_follows} follow relationships ...")
    follows = set()
    with requests.Session() as session:
        attempts = 0
        while len(follows) < args.num_follows and attempts < args.num_follows * 5:
            attempts += 1
            uid = random.randint(1, len(usernames))
            tid = random.randint(1, len(usernames))
            if uid == tid:
                continue
            key = (uid, tid)
            if key in follows:
                continue
            try:
                r = get_request(session, base_url, "/follow", {"userId": uid, "targetUserId": tid, "action": "follow"}, args.timeout)
                if r.status_code == 200:
                    follows.add(key)
                    if len(follows) % 100 == 0:
                        print(f"follow created: {len(follows)}/{args.num_follows}")
                else:
                    # log but keep going
                    pass
            except Exception:
                pass
            time.sleep(0.001)

    print(f"\nComposing {args.num_posts} posts ...")
    with requests.Session() as session:
        for i in range(args.num_posts):
            name = random.choice(usernames)
            text = random_text()
            if args.mentions and len(usernames) > 1 and random.random() < 0.5:
                # add a mention of another user and a simple URL to exercise services
                other = random.choice([u for u in usernames if u != name])
                text = f"@{other} " + text + " https://example.com/x"
            attempt = 0
            while True:
                attempt += 1
                try:
                    r = get_request(session, base_url, "/compose", {"username": name, "text": text}, args.timeout)
                    print(f"compose #{i+1} by {name}: status={r.status_code}, bytes={len(r.content)}")
                    break
                except Exception as e:
                    if attempt >= args.retries:
                        print(f"compose #{i+1} by {name}: error after {attempt} attempts: {e}")
                        break
                    backoff = 0.3 * attempt
                    print(f"compose #{i+1} by {name}: retrying in {backoff:.1f}s due to: {e}")
                    time.sleep(backoff)
            time.sleep(0.02)

    print("\nDone.")


if __name__ == "__main__":
    main()


