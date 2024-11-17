#pragma once

#define STEAL_FUNCTION_PTR(Type,ClassName,Function)\
template <typename T,auto mp>\
struct Thief_##Type {\
friend void* BODY_MACRO_COMBINE(steal_,ClassName,_,Function)()\
{\
T TPtr = mp;\
return *reinterpret_cast<void**>(&TPtr);\
}\
};\
void* BODY_MACRO_COMBINE(steal_,ClassName,_,Function)();\
template struct Thief_##Type<Type,&ClassName::Function>;

#define CALL_FUNCTION(ClassName,Function,ReturnType)\
template <typename T,auto mp>\
struct BODY_MACRO_COMBINE(Thief_,ClassName,_,Function) {\
template<typename ... Args>\
friend ReturnType BODY_MACRO_COMBINE(Call_,ClassName,_,Function)(T* Instance,Args...args)\
{\
return (Instance->*mp)(args...);\
}\
};\
ReturnType BODY_MACRO_COMBINE(Call_,ClassName,_,Function)(ClassName* Instance,...);\
template struct BODY_MACRO_COMBINE(Thief_,ClassName,_,Function)<ClassName,&ClassName::Function>;

#define GET_MEMBER(ClassName,Member,ReturnType)\
template <typename T,auto mp>\
struct BODY_MACRO_COMBINE(Thief_,ClassName,_,Member) {\
friend ReturnType BODY_MACRO_COMBINE(Get_,ClassName,_,Member)(T* Instance)\
{\
return Instance->*mp;\
}\
};\
ReturnType BODY_MACRO_COMBINE(Get_,ClassName,_,Member)(ClassName* Instance);\
template struct BODY_MACRO_COMBINE(Thief_,ClassName,_,Member)<ClassName,&ClassName::Member>;