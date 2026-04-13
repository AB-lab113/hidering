#include "cryptonote_basic_impl.h"
#include "cryptonote_format_utils.h"

namespace cryptonote
{
  const crypto::hash get_testnet_block_hash()
  {
    static const crypto::hash h = AUTO_VAL_INIT(h);
    if (!h.data[0])
      get_blob_hash(std::string("\\x01\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12\\x12"), h);
    return h;
  }
}
