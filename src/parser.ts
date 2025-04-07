export type BotInstruction =
  | { type: 'set_token'; token: string }
  | { type: 'on_message'; match: string; reply: string }
  // Instrucciones extendidas
  | { type: 'addButton'; params: string[] }
  | { type: 'addCmdReactions'; params: string[] }
  | { type: 'addEmoji'; params: string[] }
  | { type: 'addField'; params: string[] }
  | { type: 'addMessageReactions'; params: string[] }
  | { type: 'addReactions'; params: string[] }
  | { type: 'addSelectMenuOption'; params: string[] }
  | { type: 'addTextInput'; params: string[] }
  | { type: 'addTimestamp'; params: string[] }
  | { type: 'allMembersCount'; params: string[] }
  | { type: 'allowMention'; params: string[] }
  | { type: 'allowRoleMentions'; params: string[] }
  | { type: 'argsCheck'; params: string[] };

export function parseScript(code: string): BotInstruction[] {
  const lines = code
    .split('\n')
    .map(line => line.trim())
    .filter(line => line && !line.startsWith('//'));
  const instructions: BotInstruction[] = [];

  for (const line of lines) {
    if (line.startsWith('bot token')) {
      const match = line.match(/bot token\s*=\s*"(.*?)"/);
      if (match) {
        instructions.push({ type: 'set_token', token: match[1] });
      }
    } else if (line.startsWith('on message')) {
      const match = line.match(/on message\s*"(.*?)"\s*reply\s*"(.*?)"/);
      if (match) {
        instructions.push({ type: 'on_message', match: match[1], reply: match[2] });
      }
    } else {
      // Detecta instrucciones con formato: comando[param1;param2;...]
      const cmdMatch = line.match(/^(\w+)\[(.*)\]$/);
      if (cmdMatch) {
        const cmd = cmdMatch[1];
        const paramsStr = cmdMatch[2];
        const params = paramsStr.split(';').map(p => p.trim()).filter(p => p !== '');
        switch (cmd) {
          case 'addButton':
            instructions.push({ type: 'addButton', params });
            break;
          case 'addCmdReactions':
            instructions.push({ type: 'addCmdReactions', params });
            break;
          case 'addEmoji':
            instructions.push({ type: 'addEmoji', params });
            break;
          case 'addField':
            instructions.push({ type: 'addField', params });
            break;
          case 'addMessageReactions':
            instructions.push({ type: 'addMessageReactions', params });
            break;
          case 'addReactions':
            instructions.push({ type: 'addReactions', params });
            break;
          case 'addSelectMenuOption':
            instructions.push({ type: 'addSelectMenuOption', params });
            break;
          case 'addTextInput':
            instructions.push({ type: 'addTextInput', params });
            break;
          case 'addTimestamp':
            instructions.push({ type: 'addTimestamp', params });
            break;
          case 'allMembersCount':
            instructions.push({ type: 'allMembersCount', params });
            break;
          case 'allowMention':
            instructions.push({ type: 'allowMention', params });
            break;
          case 'allowRoleMentions':
            instructions.push({ type: 'allowRoleMentions', params });
            break;
          case 'argsCheck':
            instructions.push({ type: 'argsCheck', params });
            break;
          default:
            console.error(`❌ Instrucción desconocida: ${line}`);
        }
      } else {
        console.error(`❌ Instrucción desconocida: ${line}`);
      }
    }
  }
  return instructions;
}
