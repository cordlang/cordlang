import { Message } from 'discord.js';

export function noMention(message: Message): boolean {
  return message.mentions.users.size === 0;
}
