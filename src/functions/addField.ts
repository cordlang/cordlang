import { EmbedBuilder } from 'discord.js';

export function addField(embed: EmbedBuilder, params: string[]): void {
  const [name, value, inline] = params;
  embed.addFields({ name, value, inline: inline?.toLowerCase() === 'yes' });
}
