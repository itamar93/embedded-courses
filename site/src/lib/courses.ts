import { readFileSync, readdirSync, existsSync } from 'node:fs';
import { join } from 'node:path';
import { parse } from 'yaml';

export interface ModuleInfo {
  id: string;
  title: string;
  access: 'free' | 'paid';
  status?: 'planned';
}

export interface Course {
  id: string;
  title: string;
  summary: string;
  board: string;
  based_on?: string;
  modules: ModuleInfo[];
}

const COURSES_DIR = join(process.cwd(), '..', 'courses');

export function getCourses(): Course[] {
  return readdirSync(COURSES_DIR)
    .filter((d) => existsSync(join(COURSES_DIR, d, 'course.yaml')))
    .map((d) => parse(readFileSync(join(COURSES_DIR, d, 'course.yaml'), 'utf8')) as Course);
}

export function getCourse(id: string): Course | undefined {
  return getCourses().find((c) => c.id === id);
}
