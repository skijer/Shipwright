#ifndef LIBBK_EVENTS_H
#define LIBBK_EVENTS_H

// Lighthouse's enhancement hooks; libbk keeps the vanilla answer of each.
#define EventSystem_Should(id, result, ...) (result)
#define EventSystem_InvertControl(type) 1
#define CALL_EVENT(...) ((void)0)
#define CALL_CANCELLABLE_EVENT(...) if (1)

#endif
