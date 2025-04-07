import fs from 'fs';
import path from 'path';
import { parseScript, BotInstruction } from './parser';
import { runBot } from './runtime';

const [, , file] = process.argv;
if (!file) {
  console.error('❌ Archivo .cxl no especificado.');
  process.exit(1);
}

const fullPath = path.resolve(file);
const code = fs.readFileSync(fullPath, 'utf-8');

const instructions: BotInstruction[] = parseScript(code);
runBot(instructions);
