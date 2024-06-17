// TODO: this is a temporary file
#include "queue/SenderQueueItem.h"

#include "plugin/interface/Flusher.h"

namespace logtail {

SenderQueueItem::SenderQueueItem(std::string&& data,
                                 size_t rawSize,
                                 const Flusher* flusher,
                                 QueueKey key,
                                 RawDataType type,
                                 bool bufferOrNot)
    : mData(std::move(data)),
      mRawSize(rawSize),
      mType(type),
      mBufferOrNot(bufferOrNot),
      mFlusher(flusher),
      mQueueKey(key),
      mEnqueTime(time(nullptr)) {
    mConfigName = mFlusher->HasContext() ? mFlusher->GetContext().GetConfigName() : "";
}

} // namespace logtail
