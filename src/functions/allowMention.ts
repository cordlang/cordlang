import { Message } from 'discord.js';

export function allowMention(message: Message): string {
  // En este ejemplo, se devuelve el contenido original permitiendo las menciones
  return message.content;
}
