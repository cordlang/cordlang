import { ButtonBuilder, ButtonStyle } from 'discord.js';

export interface AddButtonParams {
  newRow: string;
  interaction: string;
  label: string;
  style: string;
  disable?: string;
  emoji?: string;
  messageId?: string;
}

export function addButton(params: string[]): ButtonBuilder {
  const [newRow, interaction, label, style, disable, emoji] = params;
  let buttonStyle: ButtonStyle;
  switch (style.toLowerCase()) {
    case 'primary':
      buttonStyle = ButtonStyle.Primary;
      break;
    case 'secondary':
      buttonStyle = ButtonStyle.Secondary;
      break;
    case 'success':
      buttonStyle = ButtonStyle.Success;
      break;
    case 'danger':
      buttonStyle = ButtonStyle.Danger;
      break;
    case 'link':
      buttonStyle = ButtonStyle.Link;
      break;
    default:
      buttonStyle = ButtonStyle.Primary;
  }
  const button = new ButtonBuilder()
    .setCustomId(interaction)
    .setLabel(label)
    .setStyle(buttonStyle);
  if (disable && disable.toLowerCase() === 'yes') {
    button.setDisabled(true);
  }
  if (emoji) {
    button.setEmoji(emoji);
  }
  return button;
}
