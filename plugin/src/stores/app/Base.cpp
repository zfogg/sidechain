#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

AppStore::AppStore() {
  Util::logInfo("AppStore", "Initialized app store (pure orchestration layer)");
}

void AppStore::setNetworkClient(NetworkClient *client) {
  networkClient = client;
  Util::logInfo("AppStore", "Network client configured");
}

void AppStore::setStreamChatClient(StreamChatClient *client) {
  streamChatClient = client;
  Util::logInfo("AppStore", "Stream chat client configured");
}

} // namespace Stores
} // namespace Sidechain
