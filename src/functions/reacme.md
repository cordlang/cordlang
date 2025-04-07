addMessageReactions[Channel ID;Message ID;Emojis;...]
Parameters
Channel ID (Type: Snowflake || Flag: Required): The ID of the channel where the message is located.
Message ID (Type: Snowflake || Flag: Required): The ID of the message to which the reactions will be added.
Emojis (Type: Emoji || Flag: Required): The emoji(s) to add as reaction to the message. Use semicolons ; as a separator to separate multiple emojis.
addMessageReactions[channelID;message[1];👍;✨;<:coolemoji:991742553340792882>]

addReactions[Emojis;...]
Parameters
Emojis (Type: Emoji || Flag: Required): The emoji(s) the bot reacts with. Use semicolons ; as a separator to separate multiple emojis.

addSelectMenuOption[Menu option ID;Label;Value;Description;(Default?;Emoji;Message ID)]
Parameters
Menu option ID (Type: String || Flag: Required): The ID used in newSelectMenu[].
Label (Type: String || Flag: Required): The name of the option.
Value (Type: String || Flag: Required): It’s the data that gets passed to the onInteraction[] callback. The value has to be unique in the select menu!
Description (Type: String || Flag: Emptiable): A text which shows up under the Label.
Default? (Type: Bool || Flag: Vacantable): Whether the option should be selected by default or not. Defaults to no. (yes/no) There can be only one default option!
Emoji (Type: Emoji || Flag: Vacantable): The emoji that shows up next to the Label.
Message ID (Type: String || Flag: Vacantable): The ID of a message that should have a new select menu option added to an existing select menu. By default it’s the bot’s response.

addTextInput[Text input ID;Style;Label;(Minimum length;Maximum length;Required?;Value;Placeholder)]
📌 You can add up to 5 text input fields to a modal.
Parameters
Text input ID (Type: String || Flag: Required): The ID that is used to retrieve the text input in the field. This value must be unique!
Style (Type: Enum || Flag: Required): The text input field style, either short or paragraph.
Label (Type: String || Flag: Required): The name of the text input field. This value must be less than or equal to 45 characters.
Minimum length (Type: Integer || Flag: Vacantable): Minimum number of characters a user needs to input. This value must be an integer between 0 and 4000, and can’t be greater than the Maximum length.
Maximum length (Type: Integer || Flag: Vacantable): Maximum number of characters a user can input. This value must be an integer between 0 and 4000, and can’t be less than the Minimum length.
Required? (Type: Bool || Flag: Optional): Whether a user must fill in the text input field or not. Defaults to yes. (yes/no)
Value (Type: String || Flag: Vacantable): The text that is written by default in the text input field. This value must be less than or equal to 4000 characters and must not be less than Minimum length and no more than Maximum length.
Placeholder (Type: String || Flag: Vacantable): The text that is displayed if the text input field is empty. This value must be less than or equal to 100 characters.


addTimestamp
Example
nomention
description[Hi!]
footer[That is the timestamp =>]
addTimestamp

Syntax
addTimestamp[Index]
Parameters
Index (Type: Integer || Flag: Optional): To which embed the timestamp should be added to.

Syntax
allMembersCount
Example
nomention
I'm serving allMembersCount users!

Syntax
allowMention
Example
nomention
allowMention
message
With allowMention:
RainbowKey
RainbowKey
04/06/2025
!example @RainbowKey
Bot
04/06/2025
@RainbowKey
Without allowMention
RainbowKey
RainbowKey
04/06/2025
!example @RainbowKey
Bot
04/06/2025
@RainbowKey

Syntax
allowRoleMentions[Role IDs;...]
Parameters
Role IDs (Type: Snowflake || Flag: Emptiable): The role(s) that can be pinged, leave empty to disable pings for every role. Separate role IDs using ;.
