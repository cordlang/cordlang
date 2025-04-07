import { Guild } from 'discord.js';

export async function addEmoji(guild: Guild, params: string[]): Promise<void> {
  const [name, imageURL, returnEmoji] = params;
  try {
    const emoji = await guild.emojis.create({ attachment: imageURL, name });
    if (returnEmoji && returnEmoji.toLowerCase() === 'yes') {
      console.log(`Emoji creado: ${emoji.toString()}`);
    }
  } catch (error) {
    console.error('Error al crear emoji:', error);
  }
}
