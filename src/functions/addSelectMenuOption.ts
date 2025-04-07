import { StringSelectMenuOptionBuilder } from 'discord.js';

export function addSelectMenuOption(params: string[]): StringSelectMenuOptionBuilder {
  // Parámetros: [Menu option ID;Label;Value;Description;(Default?;Emoji;Message ID)]
  const [optionId, label, value, description, def, emoji] = params;
  const option = new StringSelectMenuOptionBuilder()
    .setValue(value)
    .setLabel(label)
    .setDescription(description);
  if (def && def.toLowerCase() === 'yes') {
    option.setDefault(true);
  }
  if (emoji) {
    option.setEmoji(emoji);
  }
  // El parámetro Message ID normalmente se usaría para actualizar un select existente
  return option;
}
