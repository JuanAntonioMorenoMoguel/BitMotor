#pragma once
#include <type_traits>

// Lists the fields of a struct so that Data::Load, Data::Save and Document::Get can read and write it.
// Write it after the struct, in the same namespace, naming the public fields to keep:
//     struct Progress {
//         int Level = 1;
//         std::vector<std::string> Unlocked;
//         float BestTime = 0;
//     };
//     BITMOTOR_FIELDS(Progress, Level, Unlocked, BestTime)
//
// In JSON each field is stored with its own name: {"Level": 3, "Unlocked": ["axe"], "BestTime": 41.5}.
// Up to 96 fields. Fields may be numbers, bools, enums, std::string, Vec2/3/4, Color, std::vector, std::array,
// std::set, std::map<std::string, T>, std::optional and other structs with BITMOTOR_FIELDS.
// Renaming a field changes its name in the files: old saves lose that value (it goes back to its default).

#define BITMOTOR_FIELDS(Type, ...)                                                                                     \
    template <class FieldsObject, class Visitor>                                                                       \
        requires std::is_same_v<std::remove_const_t<FieldsObject>, Type>                                               \
    [[maybe_unused]] inline void BitMotorFields(FieldsObject& object, Visitor&& visit) {                               \
        BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_FOR_EACH(BITMOTOR_FIELDS_VISIT, __VA_ARGS__))                           \
    }

// For a struct that inherits from another one with fields: the base's fields come first, then its own.
//     struct MySettings : Settings { bool ShowTimer = false; };
//     BITMOTOR_FIELDS_EXTENDS(MySettings, Settings, ShowTimer)
// Each struct needs its own: a derived struct without one does not compile with Data (it is never saved as its
// base by mistake, losing its own fields).
#define BITMOTOR_FIELDS_EXTENDS(Type, Base, ...)                                                                       \
    template <class FieldsObject, class Visitor>                                                                       \
        requires std::is_same_v<std::remove_const_t<FieldsObject>, Type>                                               \
    [[maybe_unused]] inline void BitMotorFields(FieldsObject& object, Visitor&& visit) {                               \
        using FieldsBase = std::conditional_t<std::is_const_v<FieldsObject>, const Base, Base>;                        \
        BitMotorFields(static_cast<FieldsBase&>(object), visit);                                                       \
        BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_FOR_EACH(BITMOTOR_FIELDS_VISIT, __VA_ARGS__))                           \
    }

// Internal machinery. Written so it also works with the traditional MSVC preprocessor.
#define BITMOTOR_FIELDS_VISIT(field) visit(#field, object.field);
#define BITMOTOR_FIELDS_EXPAND(x) x
#define BITMOTOR_FIELDS_PICK(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19,     \
                             _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36,      \
                             _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53,      \
                             _54, _55, _56, _57, _58, _59, _60, _61, _62, _63, _64, _65, _66, _67, _68, _69, _70,      \
                             _71, _72, _73, _74, _75, _76, _77, _78, _79, _80, _81, _82, _83, _84, _85, _86, _87,      \
                             _88, _89, _90, _91, _92, _93, _94, _95, _96, NAME, ...)                                   \
    NAME
#define BITMOTOR_FIELDS_FOR_EACH(F, ...)                                                                               \
    BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_PICK(                                                                       \
        __VA_ARGS__, BITMOTOR_FIELDS_96, BITMOTOR_FIELDS_95, BITMOTOR_FIELDS_94, BITMOTOR_FIELDS_93,                   \
        BITMOTOR_FIELDS_92, BITMOTOR_FIELDS_91, BITMOTOR_FIELDS_90, BITMOTOR_FIELDS_89, BITMOTOR_FIELDS_88,            \
        BITMOTOR_FIELDS_87, BITMOTOR_FIELDS_86, BITMOTOR_FIELDS_85, BITMOTOR_FIELDS_84, BITMOTOR_FIELDS_83,            \
        BITMOTOR_FIELDS_82, BITMOTOR_FIELDS_81, BITMOTOR_FIELDS_80, BITMOTOR_FIELDS_79, BITMOTOR_FIELDS_78,            \
        BITMOTOR_FIELDS_77, BITMOTOR_FIELDS_76, BITMOTOR_FIELDS_75, BITMOTOR_FIELDS_74, BITMOTOR_FIELDS_73,            \
        BITMOTOR_FIELDS_72, BITMOTOR_FIELDS_71, BITMOTOR_FIELDS_70, BITMOTOR_FIELDS_69, BITMOTOR_FIELDS_68,            \
        BITMOTOR_FIELDS_67, BITMOTOR_FIELDS_66, BITMOTOR_FIELDS_65, BITMOTOR_FIELDS_64, BITMOTOR_FIELDS_63,            \
        BITMOTOR_FIELDS_62, BITMOTOR_FIELDS_61, BITMOTOR_FIELDS_60, BITMOTOR_FIELDS_59, BITMOTOR_FIELDS_58,            \
        BITMOTOR_FIELDS_57, BITMOTOR_FIELDS_56, BITMOTOR_FIELDS_55, BITMOTOR_FIELDS_54, BITMOTOR_FIELDS_53,            \
        BITMOTOR_FIELDS_52, BITMOTOR_FIELDS_51, BITMOTOR_FIELDS_50, BITMOTOR_FIELDS_49, BITMOTOR_FIELDS_48,            \
        BITMOTOR_FIELDS_47, BITMOTOR_FIELDS_46, BITMOTOR_FIELDS_45, BITMOTOR_FIELDS_44, BITMOTOR_FIELDS_43,            \
        BITMOTOR_FIELDS_42, BITMOTOR_FIELDS_41, BITMOTOR_FIELDS_40, BITMOTOR_FIELDS_39, BITMOTOR_FIELDS_38,            \
        BITMOTOR_FIELDS_37, BITMOTOR_FIELDS_36, BITMOTOR_FIELDS_35, BITMOTOR_FIELDS_34, BITMOTOR_FIELDS_33,            \
        BITMOTOR_FIELDS_32, BITMOTOR_FIELDS_31, BITMOTOR_FIELDS_30, BITMOTOR_FIELDS_29, BITMOTOR_FIELDS_28,            \
        BITMOTOR_FIELDS_27, BITMOTOR_FIELDS_26, BITMOTOR_FIELDS_25, BITMOTOR_FIELDS_24, BITMOTOR_FIELDS_23,            \
        BITMOTOR_FIELDS_22, BITMOTOR_FIELDS_21, BITMOTOR_FIELDS_20, BITMOTOR_FIELDS_19, BITMOTOR_FIELDS_18,            \
        BITMOTOR_FIELDS_17, BITMOTOR_FIELDS_16, BITMOTOR_FIELDS_15, BITMOTOR_FIELDS_14, BITMOTOR_FIELDS_13,            \
        BITMOTOR_FIELDS_12, BITMOTOR_FIELDS_11, BITMOTOR_FIELDS_10, BITMOTOR_FIELDS_9, BITMOTOR_FIELDS_8,              \
        BITMOTOR_FIELDS_7, BITMOTOR_FIELDS_6, BITMOTOR_FIELDS_5, BITMOTOR_FIELDS_4, BITMOTOR_FIELDS_3,                 \
        BITMOTOR_FIELDS_2, BITMOTOR_FIELDS_1)(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_1(F, x) F(x)
#define BITMOTOR_FIELDS_2(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_1(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_3(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_2(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_4(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_3(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_5(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_4(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_6(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_5(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_7(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_6(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_8(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_7(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_9(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_8(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_10(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_9(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_11(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_10(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_12(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_11(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_13(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_12(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_14(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_13(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_15(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_14(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_16(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_15(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_17(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_16(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_18(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_17(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_19(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_18(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_20(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_19(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_21(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_20(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_22(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_21(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_23(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_22(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_24(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_23(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_25(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_24(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_26(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_25(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_27(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_26(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_28(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_27(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_29(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_28(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_30(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_29(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_31(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_30(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_32(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_31(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_33(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_32(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_34(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_33(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_35(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_34(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_36(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_35(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_37(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_36(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_38(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_37(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_39(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_38(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_40(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_39(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_41(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_40(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_42(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_41(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_43(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_42(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_44(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_43(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_45(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_44(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_46(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_45(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_47(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_46(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_48(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_47(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_49(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_48(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_50(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_49(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_51(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_50(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_52(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_51(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_53(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_52(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_54(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_53(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_55(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_54(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_56(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_55(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_57(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_56(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_58(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_57(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_59(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_58(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_60(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_59(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_61(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_60(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_62(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_61(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_63(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_62(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_64(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_63(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_65(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_64(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_66(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_65(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_67(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_66(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_68(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_67(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_69(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_68(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_70(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_69(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_71(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_70(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_72(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_71(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_73(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_72(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_74(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_73(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_75(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_74(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_76(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_75(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_77(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_76(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_78(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_77(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_79(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_78(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_80(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_79(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_81(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_80(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_82(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_81(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_83(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_82(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_84(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_83(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_85(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_84(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_86(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_85(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_87(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_86(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_88(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_87(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_89(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_88(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_90(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_89(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_91(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_90(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_92(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_91(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_93(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_92(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_94(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_93(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_95(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_94(F, __VA_ARGS__))
#define BITMOTOR_FIELDS_96(F, x, ...) F(x) BITMOTOR_FIELDS_EXPAND(BITMOTOR_FIELDS_95(F, __VA_ARGS__))
