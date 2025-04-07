import { Message } from 'discord.js';

export function getAuthorID(message: Message): string {
  return message.author.id;
}
