import { EmbedBuilder } from 'discord.js';

export function addTimestamp(embed: EmbedBuilder, params: string[]): void {
  // Parámetros: [Index] (en este ejemplo, asumiremos que se aplica al embed actual)
  // Podrías usar el parámetro "Index" si manejas varios embeds.
  embed.setTimestamp(new Date());
}
