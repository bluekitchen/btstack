#ifndef SUBSTRATE_H_
#define SUBSTRATE_H_

#ifdef __APPLE__
#ifdef __cplusplus
extern "C" {
#endif
#include <mach-o/nlist.h>
#ifdef __cplusplus
}
#endif

#include <objc/runtime.h>
#include <objc/message.h>
#endif

#include <dlfcn.h>

#define _finline \
    inline __attribute__((always_inline))
#define _disused \
    __attribute__((unused))

#ifdef __cplusplus
#define _default(value) = value
#else
#define _default(value)
#endif

#ifdef __cplusplus
extern "C" {
#endif

void MSHookFunction(void *symbol, void *replace, void **result);

#ifdef __APPLE__
IMP MSHookMessage(Class _class, SEL sel, IMP imp, const char *prefix _default(NULL));
void MSHookMessageEx(Class _class, SEL sel, IMP imp, IMP *result);
#endif

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#ifdef __APPLE__

namespace etl {

template <unsigned Case_>
struct Case {
    static char value[Case_ + 1];
};

typedef Case<true> Yes;
typedef Case<false> No;

namespace be {
    template <typename Checked_>
    static Yes CheckClass_(void (Checked_::*)());

    template <typename Checked_>
    static No CheckClass_(...);
}

template <typename Type_>
struct IsClass {
    void gcc32();

    static const bool value = (sizeof(be::CheckClass_<Type_>(0).value) == sizeof(Yes::value));
};

}

template <typename Type_>
static inline Type_ *MSHookMessage(Class _class, SEL sel, Type_ *imp, const char *prefix = NULL) {
    return reinterpret_cast<Type_ *>(MSHookMessage(_class, sel, reinterpret_cast<IMP>(imp), prefix));
}

template <typename Type_>
static inline void MSHookMessage(Class _class, SEL sel, Type_ *imp, Type_ **result) {
    return MSHookMessageEx(_class, sel, reinterpret_cast<IMP>(imp), reinterpret_cast<IMP *>(result));
}

template <typename Type_>
static inline Type_ &MSHookIvar(id self, const char *name) {
    Ivar ivar(class_getInstanceVariable(object_getClass(self), name));
    void *pointer(ivar == NULL ? NULL : reinterpret_cast<char *>(self) + ivar_getOffset(ivar));
    return *reinterpret_cast<Type_ *>(pointer);
}

#define MSHookMessage0(_class, arg0) \
    MSHookMessage($ ## _class, @selector(arg0), MSHake(_class ## $ ## arg0))
#define MSHookMessage1(_class, arg0) \
    MSHookMessage($ ## _class, @selector(arg0:), MSHake(_class ## $ ## arg0 ## $))
#define MSHookMessage2(_class, arg0, arg1) \
    MSHookMessage($ ## _class, @selector(arg0:arg1:), MSHake(_class ## $ ## arg0 ## $ ## arg1 ## $))
#define MSHookMessage3(_class, arg0, arg1, arg2) \
    MSHookMessage($ ## _class, @selector(arg0:arg1:arg2:), MSHake(_class ## $ ## arg0 ## $ ## arg1 ## $ ## arg2 ## $))
#define MSHookMessage4(_class, arg0, arg1, arg2, arg3) \
    MSHookMessage($ ## _class, @selector(arg0:arg1:arg2:arg3:), MSHake(_class ## $ ## arg0 ## $ ## arg1 ## $ ## arg2 ## $ ## arg3 ## $))
#define MSHookMessage5(_class, arg0, arg1, arg2, arg3, arg4) \
    MSHookMessage($ ## _class, @selector(arg0:arg1:arg2:arg3:arg4:), MSHake(_class ## $ ## arg0 ## $ ## arg1 ## $ ## arg2 ## $ ## arg3 ## $ ## arg4 ## $))
#define MSHookMessage6(_class, arg0, arg1, arg2, arg3, arg4, arg5) \
    MSHookMessage($ ## _class, @selector(arg0:arg1:arg2:arg3:arg4:arg5:), MSHake(_class ## $ ## arg0 ## $ ## arg1 ## $ ## arg2 ## $ ## arg3 ## $ ## arg4 ## $ ## arg5 ## $))

#define MSRegister_(name, dollar, colon) \
    static class C_$ ## name ## $ ## dollar { public: _finline C_$ ## name ## $ ##dollar() { \
        MSHookMessage($ ## name, @selector(colon), MSHake(name ## $ ## dollar)); \
    } } V_$ ## dollar; \

#define MSIgnore_(name, dollar, colon)

#define MSMessage_(extra, type, _class, name, dollar, colon, call, args...) \
    static type _$ ## name ## $ ## dollar(Class _cls, type (*_old)(_class, SEL, ## args), type (*_spr)(struct objc_super *, SEL, ## args), _class self, SEL _cmd, ## args); \
    MSFunction(type, name ## $ ## dollar, _class self, SEL _cmd, ## args) { \
        Class const _cls($ ## name); \
        type (* const _old)(_class, SEL, ## args) = _ ## name ## $ ## dollar; \
        typedef type (*msgSendSuper_t)(struct objc_super *, SEL, ## args); \
        msgSendSuper_t const _spr(::etl::IsClass<type>::value ? reinterpret_cast<msgSendSuper_t>(&objc_msgSendSuper_stret) : reinterpret_cast<msgSendSuper_t>(&objc_msgSendSuper)); \
        return _$ ## name ## $ ## dollar call; \
    } \
    extra(name, dollar, colon) \
    static _finline type _$ ## name ## $ ## dollar(Class _cls, type (*_old)(_class, SEL, ## args), type (*_spr)(struct objc_super *, SEL, ## args), _class self, SEL _cmd, ## args)

/* for((x=1;x!=7;++x)){ echo -n "#define MSMessage${x}_(extra, type, _class, name";for((y=0;y!=x;++y));do echo -n ", sel$y";done;for((y=0;y!=x;++y));do echo -n ", type$y, arg$y";done;echo ") \\";echo -n "    MSMessage_(extra, type, _class, name,";for((y=0;y!=x;++y));do if [[ $y -ne 0 ]];then echo -n " ##";fi;echo -n " sel$y ## $";done;echo -n ", ";for((y=0;y!=x;++y));do echo -n "sel$y:";done;echo -n ", (_cls, _old, _spr, self, _cmd";for((y=0;y!=x;++y));do echo -n ", arg$y";done;echo -n ")";for((y=0;y!=x;++y));do echo -n ", type$y arg$y";done;echo ")";} */

#define MSMessage0_(extra, type, _class, name, sel0) \
    MSMessage_(extra, type, _class, name, sel0, sel0, (_cls, _old, _spr, self, _cmd))
#define MSMessage1_(extra, type, _class, name, sel0, type0, arg0) \
    MSMessage_(extra, type, _class, name, sel0 ## $, sel0:, (_cls, _old, _spr, self, _cmd, arg0), type0 arg0)
#define MSMessage2_(extra, type, _class, name, sel0, sel1, type0, arg0, type1, arg1) \
    MSMessage_(extra, type, _class, name, sel0 ## $ ## sel1 ## $, sel0:sel1:, (_cls, _old, _spr, self, _cmd, arg0, arg1), type0 arg0, type1 arg1)
#define MSMessage3_(extra, type, _class, name, sel0, sel1, sel2, type0, arg0, type1, arg1, type2, arg2) \
    MSMessage_(extra, type, _class, name, sel0 ## $ ## sel1 ## $ ## sel2 ## $, sel0:sel1:sel2:, (_cls, _old, _spr, self, _cmd, arg0, arg1, arg2), type0 arg0, type1 arg1, type2 arg2)
#define MSMessage4_(extra, type, _class, name, sel0, sel1, sel2, sel3, type0, arg0, type1, arg1, type2, arg2, type3, arg3) \
    MSMessage_(extra, type, _class, name, sel0 ## $ ## sel1 ## $ ## sel2 ## $ ## sel3 ## $, sel0:sel1:sel2:sel3:, (_cls, _old, _spr, self, _cmd, arg0, arg1, arg2, arg3), type0 arg0, type1 arg1, type2 arg2, type3 arg3)
#define MSMessage5_(extra, type, _class, name, sel0, sel1, sel2, sel3, sel4, type0, arg0, type1, arg1, type2, arg2, type3, arg3, type4, arg4) \
    MSMessage_(extra, type, _class, name, sel0 ## $ ## sel1 ## $ ## sel2 ## $ ## sel3 ## $ ## sel4 ## $, sel0:sel1:sel2:sel3:sel4:, (_cls, _old, _spr, self, _cmd, arg0, arg1, arg2, arg3, arg4), type0 arg0, type1 arg1, type2 arg2, type3 arg3, type4 arg4)
#define MSMessage6_(extra, type, _class, name, sel0, sel1, sel2, sel3, sel4, sel5, type0, arg0, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5) \
    MSMessage_(extra, type, _class, name, sel0 ## $ ## sel1 ## $ ## sel2 ## $ ## sel3 ## $ ## sel4 ## $ ## sel5 ## $, sel0:sel1:sel2:sel3:sel4:sel5:, (_cls, _old, _spr, self, _cmd, arg0, arg1, arg2, arg3, arg4, arg5), type0 arg0, type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5)

#define MSInstanceMessage0(type, _class, args...) MSMessage0_(MSIgnore_, type, _class *, _class, ## args)
#define MSInstanceMessage1(type, _class, args...) MSMessage1_(MSIgnore_, type, _class *, _class, ## args)
#define MSInstanceMessage2(type, _class, args...) MSMessage2_(MSIgnore_, type, _class *, _class, ## args)
#define MSInstanceMessage3(type, _class, args...) MSMessage3_(MSIgnore_, type, _class *, _class, ## args)
#define MSInstanceMessage4(type, _class, args...) MSMessage4_(MSIgnore_, type, _class *, _class, ## args)
#define MSInstanceMessage5(type, _class, args...) MSMessage5_(MSIgnore_, type, _class *, _class, ## args)
#define MSInstanceMessage6(type, _class, args...) MSMessage6_(MSIgnore_, type, _class *, _class, ## args)

#define MSClassMessage0(type, _class, args...) MSMessage0_(MSIgnore_, type, Class, $ ## _class, ## args)
#define MSClassMessage1(type, _class, args...) MSMessage1_(MSIgnore_, type, Class, $ ## _class, ## args)
#define MSClassMessage2(type, _class, args...) MSMessage2_(MSIgnore_, type, Class, $ ## _class, ## args)
#define MSClassMessage3(type, _class, args...) MSMessage3_(MSIgnore_, type, Class, $ ## _class, ## args)
#define MSClassMessage4(type, _class, args...) MSMessage4_(MSIgnore_, type, Class, $ ## _class, ## args)
#define MSClassMessage5(type, _class, args...) MSMessage5_(MSIgnore_, type, Class, $ ## _class, ## args)
#define MSClassMessage6(type, _class, args...) MSMessage6_(MSIgnore_, type, Class, $ ## _class, ## args)

#define MSInstanceMessageHook0(type, _class, args...) MSMessage0_(MSRegister_, type, _class *, _class, ## args)
#define MSInstanceMessageHook1(type, _class, args...) MSMessage1_(MSRegister_, type, _class *, _class, ## args)
#define MSInstanceMessageHook2(type, _class, args...) MSMessage2_(MSRegister_, type, _class *, _class, ## args)
#define MSInstanceMessageHook3(type, _class, args...) MSMessage3_(MSRegister_, type, _class *, _class, ## args)
#define MSInstanceMessageHook4(type, _class, args...) MSMessage4_(MSRegister_, type, _class *, _class, ## args)
#define MSInstanceMessageHook5(type, _class, args...) MSMessage5_(MSRegister_, type, _class *, _class, ## args)
#define MSInstanceMessageHook6(type, _class, args...) MSMessage6_(MSRegister_, type, _class *, _class, ## args)

#define MSClassMessageHook0(type, _class, args...) MSMessage0_(MSRegister_, type, Class, $ ## _class, ## args)
#define MSClassMessageHook1(type, _class, args...) MSMessage1_(MSRegister_, type, Class, $ ## _class, ## args)
#define MSClassMessageHook2(type, _class, args...) MSMessage2_(MSRegister_, type, Class, $ ## _class, ## args)
#define MSClassMessageHook3(type, _class, args...) MSMessage3_(MSRegister_, type, Class, $ ## _class, ## args)
#define MSClassMessageHook4(type, _class, args...) MSMessage4_(MSRegister_, type, Class, $ ## _class, ## args)
#define MSClassMessageHook5(type, _class, args...) MSMessage5_(MSRegister_, type, Class, $ ## _class, ## args)
#define MSClassMessageHook6(type, _class, args...) MSMessage6_(MSRegister_, type, Class, $ ## _class, ## args)

#define MSOldCall(args...) \
    _old(self, _cmd, ## args)
#define MSSuperCall(args...) \
    _spr(& (struct objc_super) {self, class_getSuperclass(_cls)}, _cmd, ## args)

#define MSIvarHook(type, name) \
    type &name(MSHookIvar<type>(self, #name))

#define MSClassHook(name) \
    static Class $ ## name = objc_getClass(#name);
#define MSMetaClassHook(name) \
    static Class $$ ## name = object_getClass($ ## name);

#endif/*__APPLE__*/

template <typename Type_>
static inline void MSHookFunction(Type_ *symbol, Type_ *replace, Type_ **result) {
    return MSHookFunction(
        reinterpret_cast<void *>(symbol),
        reinterpret_cast<void *>(replace),
        reinterpret_cast<void **>(result)
    );
}

template <typename Type_>
static inline void MSHookFunction(Type_ *symbol, Type_ *replace) {
    return MSHookFunction(symbol, replace, reinterpret_cast<Type_ **>(NULL));
}

template <typename Type_>
static inline void MSHookSymbol(Type_ *&value, const char *name, void *handle = RTLD_DEFAULT) {
    value = reinterpret_cast<Type_ *>(dlsym(handle, name));
}

#endif

#define MSFunction(type, name, args...) \
    _disused static type (*_ ## name)(args); \
    static type $ ## name(args)

#define MSHook MSFunction

#define MSHake(name) \
    &$ ## name, &_ ## name

#define Foundation_f "/System/Library/Frameworks/Foundation.framework/Foundation"
#define UIKit_f "/System/Library/Frameworks/UIKit.framework/UIKit"
#define JavaScriptCore_f "/System/Library/PrivateFrameworks/JavaScriptCore.framework/JavaScriptCore"
#define IOKit_f "/System/Library/Frameworks/IOKit.framework/IOKit"

#endif//SUBSTRATE_H_
