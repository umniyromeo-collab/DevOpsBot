#include "bot.h"

#include <iostream>
#include <cstdlib>

int main() {
    // Токен не хардкодим — берём из окружения, чтобы не утёк в git.
    //   export TELEGRAM_BOT_TOKEN="123456:AA..."
    const char* token = std::getenv("TELEGRAM_BOT_TOKEN");
    if (token == nullptr || *token == '\0') {
        std::cerr << "Не задан TELEGRAM_BOT_TOKEN. "
                     "Получите токен у @BotFather и экспортируйте его:\n"
                     "  export TELEGRAM_BOT_TOKEN=\"...\"" << std::endl;
        return 1;
    }

    telegram::Bot bot(token);
    bot.Run();
    return 0;
}