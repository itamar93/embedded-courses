// Bundle Monaco with the site instead of loading it from a CDN at run time.
// Only the editor core and C highlighting are included, which keeps the bundle small.
import * as monaco from 'monaco-editor/editor/editor.api.js';
import 'monaco-editor/languages/definitions/cpp/register.js';
import EditorWorker from 'monaco-editor/editor/editor.worker.js?worker';
import { loader } from '@monaco-editor/react';

self.MonacoEnvironment = { getWorker: () => new EditorWorker() };
loader.config({ monaco });
