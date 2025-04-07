import { Message } from 'discord.js';

export function addCmdReactions(message: Message, emojis: string): void {
  const emojiList = emojis.split(';').map(e => e.trim()).filter(e => e);
  for (const emoji of emojiList) {
    message.react(emoji).catch(console.error);
  }
}
