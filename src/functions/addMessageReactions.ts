import { Client, TextChannel } from 'discord.js';

export async function addMessageReactions(client: Client, params: string[]): Promise<void> {
  // Parámetros: [Channel ID;Message ID;Emojis;...]
  const [channelId, messageId, ...emojis] = params;
  try {
    const channel = await client.channels.fetch(channelId) as TextChannel;
    const message = await channel.messages.fetch(messageId);
    for (const emoji of emojis) {
      await message.react(emoji.trim());
    }
  } catch (error) {
    console.error('Error en addMessageReactions:', error);
  }
}
