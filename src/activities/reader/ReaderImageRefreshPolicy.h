#pragma once

struct ReaderImageRefreshPolicy {
  bool useHalfRefresh;
  int pagesUntilFullRefresh;
};

constexpr bool readerUsesOverlayGrayscaleForImage(const bool pageHasImages, const bool deviceIsX3, const bool isUc8279,
                                                  const bool overlaySupported) {
  return pageHasImages && !deviceIsX3 && isUc8279 && overlaySupported;
}

constexpr ReaderImageRefreshPolicy readerImageRefreshPolicy(const bool manualRefreshPending,
                                                            const int refreshFrequency) {
  return {manualRefreshPending, refreshFrequency};
}
