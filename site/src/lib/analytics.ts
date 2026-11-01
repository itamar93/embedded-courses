// Course funnel events. No-ops unless PostHog is configured (see .env.example).
type Props = Record<string, string | number | boolean>;

declare global {
  interface Window {
    posthog?: { capture: (event: string, props?: Props) => void };
  }
}

export function track(event: string, props: Props = {}) {
  window.posthog?.capture(event, props);
}
