import { defineCollection } from 'astro:content';
import { glob } from 'astro/loaders';
import { z } from 'astro/zod';

// Lessons live next to their labs in /courses/<course>/<module>/lesson.mdx
const lessons = defineCollection({
  loader: glob({
    pattern: '*/*/lesson.mdx',
    base: '../courses',
    generateId: ({ entry }) => entry.replace(/\/lesson\.mdx$/, ''),
  }),
  schema: z.object({
    title: z.string(),
    module: z.number(),
    access: z.enum(['free', 'paid']),
    minutes: z.number(),
    summary: z.string(),
  }),
});

export const collections = { lessons };
