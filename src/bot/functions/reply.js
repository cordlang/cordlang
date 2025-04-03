function execute(message, reply) {
    const user = message.author.username;
    const response = reply.replace("{user}", user);
    message.reply(response);
  }
  
  module.exports = { execute };
  