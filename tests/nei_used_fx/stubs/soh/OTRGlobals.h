#pragma once
// Only the application's configured render rate is replaced. The production
// frame interpolator and native matrix implementation are compiled unchanged.
class OTRGlobals {
  public:
    static OTRGlobals* Instance;
    int GetInterpolationFPS() const { return 60; }
};
