# Deploying for free

The site goes on **Cloudflare Pages** and the lab service on an **Oracle Cloud Always Free** VM, behind HTTPS on a free `sslip.io` address. Both are free; check each provider's current free-tier terms when you sign up.

```
browser ──https──> Cloudflare Pages (static site)
   │
   └─────https──> Oracle VM: Caddy ──unix socket──> runner (gcc + QEMU, no network)
```

## 1. Site on Cloudflare Pages (about 5 minutes)

1. Sign up at https://dash.cloudflare.com (free plan).
2. **Workers & Pages → Create → Pages → Connect to Git**, and pick `itamar93/embedded-courses`.
3. Build settings:
   - Framework preset: **Astro**
   - Root directory: `site`
   - Build command: `npm run build`
   - Build output directory: `dist`
   - Environment variable `NODE_VERSION` = `22`
4. Deploy. Note the address it gives you, for example `https://embedded-courses.pages.dev`.

Lessons show a placeholder in place of the lab workbench until step 3 below. Merge PR #1 first: Pages and the setup script both use the `main` branch.

## 2. Lab service on Oracle Cloud (about 30 minutes, most of it waiting)

1. Sign up at https://www.oracle.com/cloud/free/. It asks for a card to verify you; Always Free resources are not charged.
2. **Create a VM instance**:
   - Image: **Ubuntu 24.04**
   - Shape: **Ampere (VM.Standard.A1.Flex)**, 2 OCPUs, 12 GB memory
   - Download the SSH key it offers.
3. Open the web ports: on the instance page, open its **Subnet → Security List → Add Ingress Rules**: source `0.0.0.0/0`, TCP, destination ports `80,443`.
4. SSH in (`ssh -i <key> ubuntu@<public IP>`) and run:

   ```bash
   curl -fsSL https://raw.githubusercontent.com/itamar93/embedded-courses/main/deploy/oracle-setup.sh -o setup.sh
   SITE_ORIGIN=https://embedded-courses.pages.dev bash setup.sh
   ```

   While the repository is private, download the script from GitHub yourself (or copy its contents) and add `GITHUB_TOKEN=<token>`: a fine-grained token with read-only **Contents** access to this repository.

5. The script prints the service address, for example `https://152-67-1-2.sslip.io`. Check it with `curl https://152-67-1-2.sslip.io/healthz`, which answers `ok`.

## 3. Connect them

In Cloudflare Pages, add the environment variable `PUBLIC_RUNNER_URL` = the address from step 2.5, then **Retry deployment**. Run and Check now work on the lessons.

## Updating

- Site: every push to `main` redeploys automatically.
- Lab service: `cd ~/embedded-courses && git pull && sudo DOMAIN=<domain> SITE_ORIGIN=<site> docker compose -f deploy/docker-compose.yml up -d --build`

## Analytics

In Cloudflare: **Analytics & Logs → Web Analytics → Add a site**, copy the token into the Pages variable `PUBLIC_CF_ANALYTICS_TOKEN`. For the course funnel, create a free PostHog project and set `PUBLIC_POSTHOG_KEY`. Redeploy after changing variables.
