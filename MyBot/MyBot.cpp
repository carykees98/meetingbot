#include <fstream>
#include <iostream>
#include <list>
#include <sstream>

#include "MyBot.h"

#include <dpp/dpp.h>

json CONFIG = nlohmann::json::parse(std::ifstream("config.json"));

int main()
{
	dpp::cluster bot(CONFIG["token"],
		dpp::i_message_content | dpp::i_guild_message_reactions | dpp::i_guilds | dpp::i_guild_members);

	/* Output simple log messages to stdout */
	bot.on_log(dpp::utility::cout_logger());

	/* Register slash command here in on_ready */
	bot.on_ready(
		[&bot](const dpp::ready_t& event)
		{
			if (CONFIG["message-id"].is_null())
			{
				std::string messageContent = "If you would like to be pinged for our weekly meetings, please react to this message!";
				int64_t channelID = CONFIG["reactions-channel-id"].get<uint64_t>();
				dpp::message reactionMessage(dpp::snowflake(channelID), messageContent);

				bot.message_create(
					reactionMessage,
					[&bot](const dpp::confirmation_callback_t& callback)
					{
						bot.log(dpp::ll_info, "[INFO] - Sent Message with ID: " + std::to_string(callback.get<dpp::message>().id));
						CONFIG["message-id"] = uint64_t(callback.get<dpp::message>().id);
						std::ofstream("config.json") << CONFIG;
					});
			}

			if (dpp::run_once<struct register_bot_commands>())
			{
				bot.set_presence(dpp::presence(dpp::presence_status::ps_online, dpp::activity_type::at_watching, "for reactions"));
			}

			bot.log(dpp::ll_info, "Looking for reaction changes on message " + std::to_string(CONFIG["message-id"].get<uint64_t>()));
		});

	bot.on_message_reaction_add(
		[&bot](const dpp::message_reaction_add_t& event)
		{
			if (event.message_id == dpp::snowflake(CONFIG["message-id"].get<uint64_t>()))
			{
				bot.guild_member_add_role(dpp::snowflake(CONFIG["server-id"].get<uint64_t>()),
					event.reacting_user.id,
					dpp::snowflake(CONFIG["role-id"].get<uint64_t>()),
					[event, &bot](const dpp::confirmation_callback_t& callback)
					{
						if (callback.get<dpp::confirmation>().success)
						{
							bot.log(dpp::ll_info, "User \"" + event.reacting_user.format_username() + "\" has registered for meeting notifications.");
						}
						else
						{
							bot.log(dpp::ll_error, "Failed to register User \"" + event.reacting_user.format_username() + "\" for meeting notifications.");
						}
					});
			}
		});

	bot.on_message_reaction_remove(
		[&bot](const dpp::message_reaction_remove_t& event)
		{
			if (event.message_id == dpp::snowflake(CONFIG["message-id"].get<uint64_t>()))
			{
				bot.user_get(event.reacting_user_id,
					[&bot](const dpp::confirmation_callback_t& callback)
					{
						dpp::user_identified user = callback.get<dpp::user_identified>();

						bot.guild_member_remove_role(CONFIG["server-id"].get<uint64_t>(),
							user.id,
							CONFIG["role-id"].get<uint64_t>(),
							[user, &bot](const dpp::confirmation_callback_t& callback)
							{
								if (callback.get<dpp::confirmation>().success)
								{
									bot.log(dpp::ll_info, "User \"" + user.format_username() + "\" has unregistered for meeting notifications.");
								}
								else
								{
									bot.log(dpp::ll_error, "Failed to unregister User \"" + user.format_username() + "\" for meeting notifications.");
								}
							});
					});
			}
		});

	bot.start(dpp::st_wait);

	return 0;
}
