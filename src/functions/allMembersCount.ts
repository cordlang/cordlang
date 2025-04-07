import { Message } from 'discord.js';

export function allMembersCount(message: Message): number {
  return message.guild?.memberCount ?? 0;
}
