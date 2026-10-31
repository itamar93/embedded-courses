#!/usr/bin/env bash
# One-time setup of the lab service on an Oracle Cloud "Always Free" Ubuntu VM.
#
#   curl -fsSL <raw URL of this file> -o setup.sh
#   GITHUB_TOKEN=<read-only token> SITE_ORIGIN=https://<your-site>.pages.dev bash setup.sh
#
# GITHUB_TOKEN is only needed while the repository is private (a fine-grained
# token with read access to "Contents" of itamar93/embedded-courses is enough).
set -euo pipefail

: "${SITE_ORIGIN:?set SITE_ORIGIN to the site URL, for example https://embedded-courses.pages.dev}"
REPO=itamar93/embedded-courses
DIR="$HOME/embedded-courses"

echo "==> Installing Docker"
sudo apt-get update -y
sudo apt-get install -y docker.io docker-compose-v2 git iptables-persistent
sudo usermod -aG docker "$USER"

echo "==> Opening ports 80 and 443 in the VM firewall"
# Oracle's Ubuntu images reject everything except SSH in iptables
for port in 80 443; do
    sudo iptables -C INPUT -p tcp --dport "$port" -j ACCEPT 2>/dev/null ||
        sudo iptables -I INPUT 6 -m state --state NEW -p tcp --dport "$port" -j ACCEPT
done
sudo netfilter-persistent save

echo "==> Fetching the code"
if [ -d "$DIR/.git" ]; then
    git -C "$DIR" pull --ff-only
elif [ -n "${GITHUB_TOKEN:-}" ]; then
    git clone "https://x-access-token:${GITHUB_TOKEN}@github.com/${REPO}.git" "$DIR"
    git -C "$DIR" remote set-url origin "https://github.com/${REPO}.git"
else
    git clone "https://github.com/${REPO}.git" "$DIR"
fi

IP="$(curl -fsS https://ifconfig.me)"
DOMAIN="${DOMAIN:-${IP//./-}.sslip.io}"

echo "==> Building and starting (the first build compiles QEMU and takes 10-20 minutes)"
cd "$DIR"
sudo DOMAIN="$DOMAIN" SITE_ORIGIN="$SITE_ORIGIN" docker compose -f deploy/docker-compose.yml up -d --build

echo
echo "Lab service: https://${DOMAIN}"
echo "Check it with: curl https://${DOMAIN}/healthz"
echo "Set PUBLIC_RUNNER_URL=https://${DOMAIN} in Cloudflare Pages."
