import { Message } from 'discord.js';

export function sendReply(message: Message, content: string): void {
  message.reply(content);
}
