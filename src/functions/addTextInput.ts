import { TextInputBuilder, TextInputStyle } from 'discord.js';

export function addTextInput(params: string[]): TextInputBuilder {
  // Parámetros: [Text input ID;Style;Label;(Minimum length;Maximum length;Required?;Value;Placeholder)]
  const [inputId, style, label, extra] = params;
  const extras = extra.split(';').map(e => e.trim());
  const [min, max, required, value, placeholder] = extras;
  const textInput = new TextInputBuilder()
    .setCustomId(inputId)
    .setLabel(label)
    .setStyle(style.toLowerCase() === 'paragraph' ? TextInputStyle.Paragraph : TextInputStyle.Short);
  if (min) textInput.setMinLength(Number(min));
  if (max) textInput.setMaxLength(Number(max));
  if (required) textInput.setRequired(required.toLowerCase() === 'yes');
  if (value) textInput.setValue(value);
  if (placeholder) textInput.setPlaceholder(placeholder);
  return textInput;
}
