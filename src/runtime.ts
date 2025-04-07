import { Client, GatewayIntentBits, Events, Message, EmbedBuilder, ActionRowBuilder, ButtonBuilder } from 'discord.js';
import dotenv from 'dotenv';
import {
    sendReply,
    getAuthorID,
    addButton,
    addCmdReactions,
    addEmoji,
    addField,
    addMessageReactions,
    addReactions,
    addSelectMenuOption,
    addTextInput,
    addTimestamp,
    allMembersCount,
    allowMention,
    allowRoleMentions
} from './functions';
dotenv.config();

interface Handler {
    match: string;
    reply: string;
}

type ExtraInstruction =
    | { type: 'addButton'; params: string[] }
    | { type: 'addCmdReactions'; params: string[] }
    | { type: 'addEmoji'; params: string[] }
    | { type: 'addField'; params: string[] }
    | { type: 'addMessageReactions'; params: string[] }
    | { type: 'addReactions'; params: string[] }
    | { type: 'addSelectMenuOption'; params: string[] }
    | { type: 'addTextInput'; params: string[] }
    | { type: 'addTimestamp'; params: string[] }
    | { type: 'allMembersCount'; params: string[] }
    | { type: 'allowMention'; params: string[] }
    | { type: 'allowRoleMentions'; params: string[] }
    | { type: 'argsCheck'; params: string[] };

export function runBot(instructions: any[]) {
    let token = '';
    const handlers: Handler[] = [];
    const extraInstructions: ExtraInstruction[] = [];

    for (const inst of instructions) {
        if (inst.type === 'set_token') {
            token = inst.token;
        } else if (inst.type === 'on_message') {
            handlers.push({ match: inst.match, reply: inst.reply });
        } else {
            extraInstructions.push(inst);
        }
    }

    if (!token) {
        console.error('❌ Token del bot no encontrado en el archivo.');
        return;
    }

    const client = new Client({
        intents: [
            GatewayIntentBits.Guilds,
            GatewayIntentBits.GuildMessages,
            GatewayIntentBits.MessageContent
        ]
    });

    client.once(Events.ClientReady, (readyClient) => {
        console.log(`✅ Bot conectado como ${readyClient.user?.tag}`);
    });

    client.on(Events.MessageCreate, async (message: Message) => {
        if (message.author.bot) return;

        const content = message.content.trim().toLowerCase();

        // Respuestas simples
        for (const handler of handlers) {
            if (content === handler.match.toLowerCase()) {
                let replyText = handler.reply;
                replyText = replyText.replace('{autorID}', getAuthorID(message));
                replyText = replyText.replace('{timestamp}', new Date().toLocaleTimeString());
                sendReply(message, replyText);
                return;
            }
        }

        // Comando "build" para generar un embed con funciones extendidas
        if (content === 'build') {
            const embed = new EmbedBuilder().setTitle('Embed construido con CordLang');

            // Agregar campos (addField)
            for (const inst of extraInstructions) {
                if (inst.type === 'addField') {
                    addField(embed, inst.params);
                }
            }

            // Crear una ActionRow para botones (addButton)
            const actionRow = new ActionRowBuilder<ButtonBuilder>();
            for (const inst of extraInstructions) {
                if (inst.type === 'addButton') {
                    const button = addButton(inst.params);
                    actionRow.addComponents(button);
                }
            }

            // Agregar timestamp (addTimestamp)
            for (const inst of extraInstructions) {
                if (inst.type === 'addTimestamp') {
                    addTimestamp(embed, inst.params);
                }
            }

            // Agregar emojis (addEmoji)
            for (const inst of extraInstructions) {
                if (inst.type === 'addEmoji' && message.guild) {
                    await addEmoji(message.guild, inst.params);
                }
            }
            // Agregar reacciones (addReactions)
            for (const inst of extraInstructions) {
                if (inst.type === 'addReactions') {
                    await addReactions(message, inst.params.join(';'));
                }
            }
            // Agregar select menu (addSelectMenuOption)
            for (const inst of extraInstructions) {
                if (inst.type === 'addSelectMenuOption') {
                    const selectOption = addSelectMenuOption(inst.params);
                    // Aquí podrías agregar la lógica para adjuntar el select a un componente
                }
            }
            // Agregar input de texto (addTextInput)
            for (const inst of extraInstructions) {
                if (inst.type === 'addTextInput') {
                    const textInput = addTextInput(inst.params);
                    // Aquí podrías agregar la lógica para adjuntar el input a un modal
                }
            }
            // Contar miembros (allMembersCount)
            for (const inst of extraInstructions) {
                if (inst.type === 'allMembersCount') {
                    const count = allMembersCount(message);
                    embed.addFields({ name: 'Total de miembros', value: count.toString() });
                }
            }

            // Permitir menciones (allowMention)
            for (const inst of extraInstructions) {
                if (inst.type === 'allowMention') {
                    const allow = allowMention(message);
                    if (allow) {
                        embed.setDescription(`Menciones permitidas: ${allow}`);
                    }
                }
            }
            // Permitir menciones de roles (allowRoleMentions)
            for (const inst of extraInstructions) {
                if (inst.type === 'allowRoleMentions') {
                    const allow = allowRoleMentions(inst.params);
                    if (allow.length > 0) {
                        embed.setDescription(`Menciones de roles permitidas: ${allow.join(', ')}`);
                    }
                }
            }

            // Agregar reacciones a mensajes (addMessageReactions)
            for (const inst of extraInstructions) {
                if (inst.type === 'addMessageReactions') {
                    await addMessageReactions(client, inst.params);
                }
            }

            // Agregar reacciones con el bot (addCmdReactions)
            for (const inst of extraInstructions) {
                if (inst.type === 'addCmdReactions') {
                    await addCmdReactions(message, inst.params.join(';'));
                }
            }

            // Verificar que el canal es text-based y que admite send
            if (message.channel.isTextBased() && 'send' in message.channel && typeof message.channel.send === 'function') {
                await message.channel.send({ embeds: [embed], components: [actionRow] });
            } else {
                console.error('El canal no admite el método send.');
            }
            return;
        }
    });

    client.login(token);
}
