/* Core Mustache specification fixtures (https://github.com/mustache/spec).
 * Converted to C at development time.
 * Copyright (c) 2010-2022 Mustache Contributors. MIT license:
MIT License

Copyright (c) 2010-2022 Mustache Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

static const value_t spec_node_1 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_2 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_4 = { HAWK_MUS_VAL_SCALAR, HAWK_T("world"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_3_fields[] = { { HAWK_T("subject"), &spec_node_4 } };
static const value_t spec_node_3 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_3_fields, HAWK_NULL, 1 };
static const value_t spec_node_5 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_7 = { HAWK_MUS_VAL_SCALAR, HAWK_T("& \" < >"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_6_fields[] = { { HAWK_T("forbidden"), &spec_node_7 } };
static const value_t spec_node_6 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_6_fields, HAWK_NULL, 1 };
static const value_t spec_node_8 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_10 = { HAWK_MUS_VAL_SCALAR, HAWK_T("& \" < >"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_9_fields[] = { { HAWK_T("forbidden"), &spec_node_10 } };
static const value_t spec_node_9 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_9_fields, HAWK_NULL, 1 };
static const value_t spec_node_11 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_13 = { HAWK_MUS_VAL_SCALAR, HAWK_T("& \" < >"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_12_fields[] = { { HAWK_T("forbidden"), &spec_node_13 } };
static const value_t spec_node_12 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_12_fields, HAWK_NULL, 1 };
static const value_t spec_node_14 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_16 = { HAWK_MUS_VAL_SCALAR, HAWK_T("85"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_15_fields[] = { { HAWK_T("mph"), &spec_node_16 } };
static const value_t spec_node_15 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_15_fields, HAWK_NULL, 1 };
static const value_t spec_node_17 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_19 = { HAWK_MUS_VAL_SCALAR, HAWK_T("85"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_18_fields[] = { { HAWK_T("mph"), &spec_node_19 } };
static const value_t spec_node_18 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_18_fields, HAWK_NULL, 1 };
static const value_t spec_node_20 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_22 = { HAWK_MUS_VAL_SCALAR, HAWK_T("85"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_21_fields[] = { { HAWK_T("mph"), &spec_node_22 } };
static const value_t spec_node_21 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_21_fields, HAWK_NULL, 1 };
static const value_t spec_node_23 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_25 = { HAWK_MUS_VAL_SCALAR, HAWK_T("1.21"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_24_fields[] = { { HAWK_T("power"), &spec_node_25 } };
static const value_t spec_node_24 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_24_fields, HAWK_NULL, 1 };
static const value_t spec_node_26 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_28 = { HAWK_MUS_VAL_SCALAR, HAWK_T("1.21"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_27_fields[] = { { HAWK_T("power"), &spec_node_28 } };
static const value_t spec_node_27 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_27_fields, HAWK_NULL, 1 };
static const value_t spec_node_29 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_31 = { HAWK_MUS_VAL_SCALAR, HAWK_T("1.21"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_30_fields[] = { { HAWK_T("power"), &spec_node_31 } };
static const value_t spec_node_30 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_30_fields, HAWK_NULL, 1 };
static const value_t spec_node_32 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_34 = { HAWK_MUS_VAL_NIL, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_33_fields[] = { { HAWK_T("cannot"), &spec_node_34 } };
static const value_t spec_node_33 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_33_fields, HAWK_NULL, 1 };
static const value_t spec_node_35 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_37 = { HAWK_MUS_VAL_NIL, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_36_fields[] = { { HAWK_T("cannot"), &spec_node_37 } };
static const value_t spec_node_36 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_36_fields, HAWK_NULL, 1 };
static const value_t spec_node_38 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_40 = { HAWK_MUS_VAL_NIL, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_39_fields[] = { { HAWK_T("cannot"), &spec_node_40 } };
static const value_t spec_node_39 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_39_fields, HAWK_NULL, 1 };
static const value_t spec_node_41 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_42 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_43 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_44 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_45 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_46 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_47 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_50 = { HAWK_MUS_VAL_SCALAR, HAWK_T("Joe"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_49_fields[] = { { HAWK_T("name"), &spec_node_50 } };
static const value_t spec_node_49 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_49_fields, HAWK_NULL, 1 };
static const field_t spec_node_48_fields[] = { { HAWK_T("person"), &spec_node_49 } };
static const value_t spec_node_48 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_48_fields, HAWK_NULL, 1 };
static const value_t spec_node_51 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_54 = { HAWK_MUS_VAL_SCALAR, HAWK_T("Joe"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_53_fields[] = { { HAWK_T("name"), &spec_node_54 } };
static const value_t spec_node_53 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_53_fields, HAWK_NULL, 1 };
static const field_t spec_node_52_fields[] = { { HAWK_T("person"), &spec_node_53 } };
static const value_t spec_node_52 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_52_fields, HAWK_NULL, 1 };
static const value_t spec_node_55 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_58 = { HAWK_MUS_VAL_SCALAR, HAWK_T("Joe"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_57_fields[] = { { HAWK_T("name"), &spec_node_58 } };
static const value_t spec_node_57 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_57_fields, HAWK_NULL, 1 };
static const field_t spec_node_56_fields[] = { { HAWK_T("person"), &spec_node_57 } };
static const value_t spec_node_56 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_56_fields, HAWK_NULL, 1 };
static const value_t spec_node_59 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_66 = { HAWK_MUS_VAL_SCALAR, HAWK_T("Phil"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_65_fields[] = { { HAWK_T("name"), &spec_node_66 } };
static const value_t spec_node_65 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_65_fields, HAWK_NULL, 1 };
static const field_t spec_node_64_fields[] = { { HAWK_T("e"), &spec_node_65 } };
static const value_t spec_node_64 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_64_fields, HAWK_NULL, 1 };
static const field_t spec_node_63_fields[] = { { HAWK_T("d"), &spec_node_64 } };
static const value_t spec_node_63 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_63_fields, HAWK_NULL, 1 };
static const field_t spec_node_62_fields[] = { { HAWK_T("c"), &spec_node_63 } };
static const value_t spec_node_62 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_62_fields, HAWK_NULL, 1 };
static const field_t spec_node_61_fields[] = { { HAWK_T("b"), &spec_node_62 } };
static const value_t spec_node_61 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_61_fields, HAWK_NULL, 1 };
static const field_t spec_node_60_fields[] = { { HAWK_T("a"), &spec_node_61 } };
static const value_t spec_node_60 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_60_fields, HAWK_NULL, 1 };
static const value_t spec_node_67 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_69 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_68_fields[] = { { HAWK_T("a"), &spec_node_69 } };
static const value_t spec_node_68 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_68_fields, HAWK_NULL, 1 };
static const value_t spec_node_70 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_73 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_72_fields[] = { { HAWK_T("b"), &spec_node_73 } };
static const value_t spec_node_72 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_72_fields, HAWK_NULL, 1 };
static const value_t spec_node_75 = { HAWK_MUS_VAL_SCALAR, HAWK_T("Jim"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_74_fields[] = { { HAWK_T("name"), &spec_node_75 } };
static const value_t spec_node_74 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_74_fields, HAWK_NULL, 1 };
static const field_t spec_node_71_fields[] = { { HAWK_T("a"), &spec_node_72 }, { HAWK_T("c"), &spec_node_74 } };
static const value_t spec_node_71 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_71_fields, HAWK_NULL, 2 };
static const value_t spec_node_76 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_83 = { HAWK_MUS_VAL_SCALAR, HAWK_T("Phil"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_82_fields[] = { { HAWK_T("name"), &spec_node_83 } };
static const value_t spec_node_82 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_82_fields, HAWK_NULL, 1 };
static const field_t spec_node_81_fields[] = { { HAWK_T("e"), &spec_node_82 } };
static const value_t spec_node_81 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_81_fields, HAWK_NULL, 1 };
static const field_t spec_node_80_fields[] = { { HAWK_T("d"), &spec_node_81 } };
static const value_t spec_node_80 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_80_fields, HAWK_NULL, 1 };
static const field_t spec_node_79_fields[] = { { HAWK_T("c"), &spec_node_80 } };
static const value_t spec_node_79 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_79_fields, HAWK_NULL, 1 };
static const field_t spec_node_78_fields[] = { { HAWK_T("b"), &spec_node_79 } };
static const value_t spec_node_78 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_78_fields, HAWK_NULL, 1 };
static const value_t spec_node_88 = { HAWK_MUS_VAL_SCALAR, HAWK_T("Wrong"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_87_fields[] = { { HAWK_T("name"), &spec_node_88 } };
static const value_t spec_node_87 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_87_fields, HAWK_NULL, 1 };
static const field_t spec_node_86_fields[] = { { HAWK_T("e"), &spec_node_87 } };
static const value_t spec_node_86 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_86_fields, HAWK_NULL, 1 };
static const field_t spec_node_85_fields[] = { { HAWK_T("d"), &spec_node_86 } };
static const value_t spec_node_85 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_85_fields, HAWK_NULL, 1 };
static const field_t spec_node_84_fields[] = { { HAWK_T("c"), &spec_node_85 } };
static const value_t spec_node_84 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_84_fields, HAWK_NULL, 1 };
static const field_t spec_node_77_fields[] = { { HAWK_T("a"), &spec_node_78 }, { HAWK_T("b"), &spec_node_84 } };
static const value_t spec_node_77 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_77_fields, HAWK_NULL, 2 };
static const value_t spec_node_89 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_92 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_91_fields[] = { { HAWK_T("b"), &spec_node_92 } };
static const value_t spec_node_91 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_91_fields, HAWK_NULL, 1 };
static const value_t spec_node_94 = { HAWK_MUS_VAL_SCALAR, HAWK_T("ERROR"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_93_fields[] = { { HAWK_T("c"), &spec_node_94 } };
static const value_t spec_node_93 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_93_fields, HAWK_NULL, 1 };
static const field_t spec_node_90_fields[] = { { HAWK_T("a"), &spec_node_91 }, { HAWK_T("b"), &spec_node_93 } };
static const value_t spec_node_90 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_90_fields, HAWK_NULL, 2 };
static const value_t spec_node_95 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_96 = { HAWK_MUS_VAL_SCALAR, HAWK_T("world"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_97 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_98 = { HAWK_MUS_VAL_SCALAR, HAWK_T("& \" < >"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_99 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_100 = { HAWK_MUS_VAL_SCALAR, HAWK_T("& \" < >"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_101 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_102 = { HAWK_MUS_VAL_SCALAR, HAWK_T("& \" < >"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_103 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_104 = { HAWK_MUS_VAL_SCALAR, HAWK_T("85"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_105 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_107 = { HAWK_MUS_VAL_SCALAR, HAWK_T("---"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_106_fields[] = { { HAWK_T("string"), &spec_node_107 } };
static const value_t spec_node_106 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_106_fields, HAWK_NULL, 1 };
static const value_t spec_node_108 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_110 = { HAWK_MUS_VAL_SCALAR, HAWK_T("---"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_109_fields[] = { { HAWK_T("string"), &spec_node_110 } };
static const value_t spec_node_109 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_109_fields, HAWK_NULL, 1 };
static const value_t spec_node_111 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_113 = { HAWK_MUS_VAL_SCALAR, HAWK_T("---"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_112_fields[] = { { HAWK_T("string"), &spec_node_113 } };
static const value_t spec_node_112 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_112_fields, HAWK_NULL, 1 };
static const value_t spec_node_114 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_116 = { HAWK_MUS_VAL_SCALAR, HAWK_T("---"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_115_fields[] = { { HAWK_T("string"), &spec_node_116 } };
static const value_t spec_node_115 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_115_fields, HAWK_NULL, 1 };
static const value_t spec_node_117 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_119 = { HAWK_MUS_VAL_SCALAR, HAWK_T("---"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_118_fields[] = { { HAWK_T("string"), &spec_node_119 } };
static const value_t spec_node_118 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_118_fields, HAWK_NULL, 1 };
static const value_t spec_node_120 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_122 = { HAWK_MUS_VAL_SCALAR, HAWK_T("---"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_121_fields[] = { { HAWK_T("string"), &spec_node_122 } };
static const value_t spec_node_121 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_121_fields, HAWK_NULL, 1 };
static const value_t spec_node_123 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_125 = { HAWK_MUS_VAL_SCALAR, HAWK_T("---"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_124_fields[] = { { HAWK_T("string"), &spec_node_125 } };
static const value_t spec_node_124 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_124_fields, HAWK_NULL, 1 };
static const value_t spec_node_126 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_128 = { HAWK_MUS_VAL_SCALAR, HAWK_T("---"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_127_fields[] = { { HAWK_T("string"), &spec_node_128 } };
static const value_t spec_node_127 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_127_fields, HAWK_NULL, 1 };
static const value_t spec_node_129 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_131 = { HAWK_MUS_VAL_SCALAR, HAWK_T("---"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_130_fields[] = { { HAWK_T("string"), &spec_node_131 } };
static const value_t spec_node_130 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_130_fields, HAWK_NULL, 1 };
static const value_t spec_node_132 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_134 = { HAWK_MUS_VAL_TRUE, HAWK_T("true"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_133_fields[] = { { HAWK_T("boolean"), &spec_node_134 } };
static const value_t spec_node_133 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_133_fields, HAWK_NULL, 1 };
static const value_t spec_node_135 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_137 = { HAWK_MUS_VAL_FALSE, HAWK_T("false"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_136_fields[] = { { HAWK_T("boolean"), &spec_node_137 } };
static const value_t spec_node_136 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_136_fields, HAWK_NULL, 1 };
static const value_t spec_node_138 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_140 = { HAWK_MUS_VAL_NIL, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_139_fields[] = { { HAWK_T("null"), &spec_node_140 } };
static const value_t spec_node_139 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_139_fields, HAWK_NULL, 1 };
static const value_t spec_node_141 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_144 = { HAWK_MUS_VAL_SCALAR, HAWK_T("Joe"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_143_fields[] = { { HAWK_T("name"), &spec_node_144 } };
static const value_t spec_node_143 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_143_fields, HAWK_NULL, 1 };
static const field_t spec_node_142_fields[] = { { HAWK_T("context"), &spec_node_143 } };
static const value_t spec_node_142 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_142_fields, HAWK_NULL, 1 };
static const value_t spec_node_145 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_147 = { HAWK_MUS_VAL_SCALAR, HAWK_T("foo"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_148 = { HAWK_MUS_VAL_SCALAR, HAWK_T("wrong"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_150 = { HAWK_MUS_VAL_SCALAR, HAWK_T("bar"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_149_fields[] = { { HAWK_T("b"), &spec_node_150 } };
static const value_t spec_node_149 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_149_fields, HAWK_NULL, 1 };
static const value_t spec_node_152 = { HAWK_MUS_VAL_SCALAR, HAWK_T("baz"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_151_fields[] = { { HAWK_T("d"), &spec_node_152 } };
static const value_t spec_node_151 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_151_fields, HAWK_NULL, 1 };
static const field_t spec_node_146_fields[] = { { HAWK_T("a"), &spec_node_147 }, { HAWK_T("b"), &spec_node_148 }, { HAWK_T("sec"), &spec_node_149 }, { HAWK_T("c"), &spec_node_151 } };
static const value_t spec_node_146 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_146_fields, HAWK_NULL, 4 };
static const value_t spec_node_153 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_155 = { HAWK_MUS_VAL_SCALAR, HAWK_T("bar"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_154_fields[] = { { HAWK_T("foo"), &spec_node_155 } };
static const value_t spec_node_154 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_154_fields, HAWK_NULL, 1 };
static const value_t spec_node_156 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_161 = { HAWK_MUS_VAL_SCALAR, HAWK_T("A"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_162 = { HAWK_MUS_VAL_SCALAR, HAWK_T("a"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_160_fields[] = { { HAWK_T("upper"), &spec_node_161 }, { HAWK_T("lower"), &spec_node_162 } };
static const value_t spec_node_160 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_160_fields, HAWK_NULL, 2 };
static const value_t spec_node_165 = { HAWK_MUS_VAL_SCALAR, HAWK_T("1"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_168 = { HAWK_MUS_VAL_SCALAR, HAWK_T("x"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_167_fields[] = { { HAWK_T("bname"), &spec_node_168 } };
static const value_t spec_node_167 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_167_fields, HAWK_NULL, 1 };
static const value_t spec_node_170 = { HAWK_MUS_VAL_SCALAR, HAWK_T("y"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_169_fields[] = { { HAWK_T("bname"), &spec_node_170 } };
static const value_t spec_node_169 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_169_fields, HAWK_NULL, 1 };
static const value_t* const spec_node_166_items[] = { &spec_node_167, &spec_node_169 };
static const value_t spec_node_166 = { HAWK_MUS_VAL_ARR, HAWK_T(""), HAWK_NULL, spec_node_166_items, 2 };
static const field_t spec_node_164_fields[] = { { HAWK_T("mname"), &spec_node_165 }, { HAWK_T("bottoms"), &spec_node_166 } };
static const value_t spec_node_164 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_164_fields, HAWK_NULL, 2 };
static const value_t* const spec_node_163_items[] = { &spec_node_164 };
static const value_t spec_node_163 = { HAWK_MUS_VAL_ARR, HAWK_T(""), HAWK_NULL, spec_node_163_items, 1 };
static const field_t spec_node_159_fields[] = { { HAWK_T("tname"), &spec_node_160 }, { HAWK_T("middles"), &spec_node_163 } };
static const value_t spec_node_159 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_159_fields, HAWK_NULL, 2 };
static const value_t* const spec_node_158_items[] = { &spec_node_159 };
static const value_t spec_node_158 = { HAWK_MUS_VAL_ARR, HAWK_T(""), HAWK_NULL, spec_node_158_items, 1 };
static const field_t spec_node_157_fields[] = { { HAWK_T("tops"), &spec_node_158 } };
static const value_t spec_node_157 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_157_fields, HAWK_NULL, 1 };
static const value_t spec_node_171 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_174 = { HAWK_MUS_VAL_SCALAR, HAWK_T("1"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_173_fields[] = { { HAWK_T("one"), &spec_node_174 } };
static const value_t spec_node_173 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_173_fields, HAWK_NULL, 1 };
static const value_t spec_node_176 = { HAWK_MUS_VAL_SCALAR, HAWK_T("2"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_175_fields[] = { { HAWK_T("two"), &spec_node_176 } };
static const value_t spec_node_175 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_175_fields, HAWK_NULL, 1 };
static const value_t spec_node_178 = { HAWK_MUS_VAL_SCALAR, HAWK_T("3"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_180 = { HAWK_MUS_VAL_SCALAR, HAWK_T("4"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_181 = { HAWK_MUS_VAL_SCALAR, HAWK_T("5"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_179_fields[] = { { HAWK_T("four"), &spec_node_180 }, { HAWK_T("five"), &spec_node_181 } };
static const value_t spec_node_179 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_179_fields, HAWK_NULL, 2 };
static const field_t spec_node_177_fields[] = { { HAWK_T("three"), &spec_node_178 }, { HAWK_T("d"), &spec_node_179 } };
static const value_t spec_node_177 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_177_fields, HAWK_NULL, 2 };
static const field_t spec_node_172_fields[] = { { HAWK_T("a"), &spec_node_173 }, { HAWK_T("b"), &spec_node_175 }, { HAWK_T("c"), &spec_node_177 } };
static const value_t spec_node_172 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_172_fields, HAWK_NULL, 3 };
static const value_t spec_node_182 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_186 = { HAWK_MUS_VAL_SCALAR, HAWK_T("1"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_185_fields[] = { { HAWK_T("item"), &spec_node_186 } };
static const value_t spec_node_185 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_185_fields, HAWK_NULL, 1 };
static const value_t spec_node_188 = { HAWK_MUS_VAL_SCALAR, HAWK_T("2"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_187_fields[] = { { HAWK_T("item"), &spec_node_188 } };
static const value_t spec_node_187 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_187_fields, HAWK_NULL, 1 };
static const value_t spec_node_190 = { HAWK_MUS_VAL_SCALAR, HAWK_T("3"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_189_fields[] = { { HAWK_T("item"), &spec_node_190 } };
static const value_t spec_node_189 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_189_fields, HAWK_NULL, 1 };
static const value_t* const spec_node_184_items[] = { &spec_node_185, &spec_node_187, &spec_node_189 };
static const value_t spec_node_184 = { HAWK_MUS_VAL_ARR, HAWK_T(""), HAWK_NULL, spec_node_184_items, 3 };
static const field_t spec_node_183_fields[] = { { HAWK_T("list"), &spec_node_184 } };
static const value_t spec_node_183 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_183_fields, HAWK_NULL, 1 };
static const value_t spec_node_191 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_193 = { HAWK_MUS_VAL_ARR, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_192_fields[] = { { HAWK_T("list"), &spec_node_193 } };
static const value_t spec_node_192 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_192_fields, HAWK_NULL, 1 };
static const value_t spec_node_194 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_196 = { HAWK_MUS_VAL_TRUE, HAWK_T("true"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_197 = { HAWK_MUS_VAL_SCALAR, HAWK_T("second"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_195_fields[] = { { HAWK_T("bool"), &spec_node_196 }, { HAWK_T("two"), &spec_node_197 } };
static const value_t spec_node_195 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_195_fields, HAWK_NULL, 2 };
static const value_t spec_node_198 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_200 = { HAWK_MUS_VAL_TRUE, HAWK_T("true"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_199_fields[] = { { HAWK_T("bool"), &spec_node_200 } };
static const value_t spec_node_199 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_199_fields, HAWK_NULL, 1 };
static const value_t spec_node_201 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_203 = { HAWK_MUS_VAL_FALSE, HAWK_T("false"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_202_fields[] = { { HAWK_T("bool"), &spec_node_203 } };
static const value_t spec_node_202 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_202_fields, HAWK_NULL, 1 };
static const value_t spec_node_204 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_205 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_206 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_209 = { HAWK_MUS_VAL_SCALAR, HAWK_T("a"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_210 = { HAWK_MUS_VAL_SCALAR, HAWK_T("b"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_211 = { HAWK_MUS_VAL_SCALAR, HAWK_T("c"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_212 = { HAWK_MUS_VAL_SCALAR, HAWK_T("d"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_213 = { HAWK_MUS_VAL_SCALAR, HAWK_T("e"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t* const spec_node_208_items[] = { &spec_node_209, &spec_node_210, &spec_node_211, &spec_node_212, &spec_node_213 };
static const value_t spec_node_208 = { HAWK_MUS_VAL_ARR, HAWK_T(""), HAWK_NULL, spec_node_208_items, 5 };
static const field_t spec_node_207_fields[] = { { HAWK_T("list"), &spec_node_208 } };
static const value_t spec_node_207 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_207_fields, HAWK_NULL, 1 };
static const value_t spec_node_214 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_217 = { HAWK_MUS_VAL_SCALAR, HAWK_T("1"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_218 = { HAWK_MUS_VAL_SCALAR, HAWK_T("2"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_219 = { HAWK_MUS_VAL_SCALAR, HAWK_T("3"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_220 = { HAWK_MUS_VAL_SCALAR, HAWK_T("4"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_221 = { HAWK_MUS_VAL_SCALAR, HAWK_T("5"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t* const spec_node_216_items[] = { &spec_node_217, &spec_node_218, &spec_node_219, &spec_node_220, &spec_node_221 };
static const value_t spec_node_216 = { HAWK_MUS_VAL_ARR, HAWK_T(""), HAWK_NULL, spec_node_216_items, 5 };
static const field_t spec_node_215_fields[] = { { HAWK_T("list"), &spec_node_216 } };
static const value_t spec_node_215 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_215_fields, HAWK_NULL, 1 };
static const value_t spec_node_222 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_225 = { HAWK_MUS_VAL_SCALAR, HAWK_T("1.1"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_226 = { HAWK_MUS_VAL_SCALAR, HAWK_T("2.2"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_227 = { HAWK_MUS_VAL_SCALAR, HAWK_T("3.3"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_228 = { HAWK_MUS_VAL_SCALAR, HAWK_T("4.4"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_229 = { HAWK_MUS_VAL_SCALAR, HAWK_T("5.5"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t* const spec_node_224_items[] = { &spec_node_225, &spec_node_226, &spec_node_227, &spec_node_228, &spec_node_229 };
static const value_t spec_node_224 = { HAWK_MUS_VAL_ARR, HAWK_T(""), HAWK_NULL, spec_node_224_items, 5 };
static const field_t spec_node_223_fields[] = { { HAWK_T("list"), &spec_node_224 } };
static const value_t spec_node_223 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_223_fields, HAWK_NULL, 1 };
static const value_t spec_node_230 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_234 = { HAWK_MUS_VAL_SCALAR, HAWK_T("1"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_235 = { HAWK_MUS_VAL_SCALAR, HAWK_T("2"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_236 = { HAWK_MUS_VAL_SCALAR, HAWK_T("3"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t* const spec_node_233_items[] = { &spec_node_234, &spec_node_235, &spec_node_236 };
static const value_t spec_node_233 = { HAWK_MUS_VAL_ARR, HAWK_T(""), HAWK_NULL, spec_node_233_items, 3 };
static const value_t spec_node_238 = { HAWK_MUS_VAL_SCALAR, HAWK_T("a"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_239 = { HAWK_MUS_VAL_SCALAR, HAWK_T("b"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_240 = { HAWK_MUS_VAL_SCALAR, HAWK_T("c"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t* const spec_node_237_items[] = { &spec_node_238, &spec_node_239, &spec_node_240 };
static const value_t spec_node_237 = { HAWK_MUS_VAL_ARR, HAWK_T(""), HAWK_NULL, spec_node_237_items, 3 };
static const value_t* const spec_node_232_items[] = { &spec_node_233, &spec_node_237 };
static const value_t spec_node_232 = { HAWK_MUS_VAL_ARR, HAWK_T(""), HAWK_NULL, spec_node_232_items, 2 };
static const field_t spec_node_231_fields[] = { { HAWK_T("list"), &spec_node_232 } };
static const value_t spec_node_231 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_231_fields, HAWK_NULL, 1 };
static const value_t spec_node_241 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_244 = { HAWK_MUS_VAL_SCALAR, HAWK_T("&"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_245 = { HAWK_MUS_VAL_SCALAR, HAWK_T("\""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_246 = { HAWK_MUS_VAL_SCALAR, HAWK_T("<"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_247 = { HAWK_MUS_VAL_SCALAR, HAWK_T(">"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t* const spec_node_243_items[] = { &spec_node_244, &spec_node_245, &spec_node_246, &spec_node_247 };
static const value_t spec_node_243 = { HAWK_MUS_VAL_ARR, HAWK_T(""), HAWK_NULL, spec_node_243_items, 4 };
static const field_t spec_node_242_fields[] = { { HAWK_T("list"), &spec_node_243 } };
static const value_t spec_node_242 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_242_fields, HAWK_NULL, 1 };
static const value_t spec_node_248 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_251 = { HAWK_MUS_VAL_SCALAR, HAWK_T("&"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_252 = { HAWK_MUS_VAL_SCALAR, HAWK_T("\""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_253 = { HAWK_MUS_VAL_SCALAR, HAWK_T("<"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_254 = { HAWK_MUS_VAL_SCALAR, HAWK_T(">"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t* const spec_node_250_items[] = { &spec_node_251, &spec_node_252, &spec_node_253, &spec_node_254 };
static const value_t spec_node_250 = { HAWK_MUS_VAL_ARR, HAWK_T(""), HAWK_NULL, spec_node_250_items, 4 };
static const field_t spec_node_249_fields[] = { { HAWK_T("list"), &spec_node_250 } };
static const value_t spec_node_249 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_249_fields, HAWK_NULL, 1 };
static const value_t spec_node_255 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_258 = { HAWK_MUS_VAL_SCALAR, HAWK_T("&"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_259 = { HAWK_MUS_VAL_SCALAR, HAWK_T("\""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_260 = { HAWK_MUS_VAL_SCALAR, HAWK_T("<"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_261 = { HAWK_MUS_VAL_SCALAR, HAWK_T(">"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t* const spec_node_257_items[] = { &spec_node_258, &spec_node_259, &spec_node_260, &spec_node_261 };
static const value_t spec_node_257 = { HAWK_MUS_VAL_ARR, HAWK_T(""), HAWK_NULL, spec_node_257_items, 4 };
static const field_t spec_node_256_fields[] = { { HAWK_T("list"), &spec_node_257 } };
static const value_t spec_node_256 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_256_fields, HAWK_NULL, 1 };
static const value_t spec_node_262 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_265 = { HAWK_MUS_VAL_SCALAR, HAWK_T("a"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_264_fields[] = { { HAWK_T("value"), &spec_node_265 } };
static const value_t spec_node_264 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_264_fields, HAWK_NULL, 1 };
static const value_t spec_node_267 = { HAWK_MUS_VAL_SCALAR, HAWK_T("b"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_266_fields[] = { { HAWK_T("value"), &spec_node_267 } };
static const value_t spec_node_266 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_266_fields, HAWK_NULL, 1 };
static const value_t* const spec_node_263_items[] = { &spec_node_264, &spec_node_266 };
static const value_t spec_node_263 = { HAWK_MUS_VAL_ARR, HAWK_T(""), HAWK_NULL, spec_node_263_items, 2 };
static const value_t spec_node_268 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_272 = { HAWK_MUS_VAL_TRUE, HAWK_T("true"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_271_fields[] = { { HAWK_T("c"), &spec_node_272 } };
static const value_t spec_node_271 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_271_fields, HAWK_NULL, 1 };
static const field_t spec_node_270_fields[] = { { HAWK_T("b"), &spec_node_271 } };
static const value_t spec_node_270 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_270_fields, HAWK_NULL, 1 };
static const field_t spec_node_269_fields[] = { { HAWK_T("a"), &spec_node_270 } };
static const value_t spec_node_269 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_269_fields, HAWK_NULL, 1 };
static const value_t spec_node_273 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_277 = { HAWK_MUS_VAL_FALSE, HAWK_T("false"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_276_fields[] = { { HAWK_T("c"), &spec_node_277 } };
static const value_t spec_node_276 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_276_fields, HAWK_NULL, 1 };
static const field_t spec_node_275_fields[] = { { HAWK_T("b"), &spec_node_276 } };
static const value_t spec_node_275 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_275_fields, HAWK_NULL, 1 };
static const field_t spec_node_274_fields[] = { { HAWK_T("a"), &spec_node_275 } };
static const value_t spec_node_274 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_274_fields, HAWK_NULL, 1 };
static const value_t spec_node_278 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_280 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_279_fields[] = { { HAWK_T("a"), &spec_node_280 } };
static const value_t spec_node_279 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_279_fields, HAWK_NULL, 1 };
static const value_t spec_node_281 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_283 = { HAWK_MUS_VAL_TRUE, HAWK_T("true"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_282_fields[] = { { HAWK_T("boolean"), &spec_node_283 } };
static const value_t spec_node_282 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_282_fields, HAWK_NULL, 1 };
static const value_t spec_node_284 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_286 = { HAWK_MUS_VAL_TRUE, HAWK_T("true"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_285_fields[] = { { HAWK_T("boolean"), &spec_node_286 } };
static const value_t spec_node_285 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_285_fields, HAWK_NULL, 1 };
static const value_t spec_node_287 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_289 = { HAWK_MUS_VAL_TRUE, HAWK_T("true"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_288_fields[] = { { HAWK_T("boolean"), &spec_node_289 } };
static const value_t spec_node_288 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_288_fields, HAWK_NULL, 1 };
static const value_t spec_node_290 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_292 = { HAWK_MUS_VAL_TRUE, HAWK_T("true"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_291_fields[] = { { HAWK_T("boolean"), &spec_node_292 } };
static const value_t spec_node_291 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_291_fields, HAWK_NULL, 1 };
static const value_t spec_node_293 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_295 = { HAWK_MUS_VAL_TRUE, HAWK_T("true"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_294_fields[] = { { HAWK_T("boolean"), &spec_node_295 } };
static const value_t spec_node_294 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_294_fields, HAWK_NULL, 1 };
static const value_t spec_node_296 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_298 = { HAWK_MUS_VAL_TRUE, HAWK_T("true"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_297_fields[] = { { HAWK_T("boolean"), &spec_node_298 } };
static const value_t spec_node_297 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_297_fields, HAWK_NULL, 1 };
static const value_t spec_node_299 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_301 = { HAWK_MUS_VAL_TRUE, HAWK_T("true"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_300_fields[] = { { HAWK_T("boolean"), &spec_node_301 } };
static const value_t spec_node_300 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_300_fields, HAWK_NULL, 1 };
static const value_t spec_node_302 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_304 = { HAWK_MUS_VAL_TRUE, HAWK_T("true"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_303_fields[] = { { HAWK_T("boolean"), &spec_node_304 } };
static const value_t spec_node_303 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_303_fields, HAWK_NULL, 1 };
static const value_t spec_node_305 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_307 = { HAWK_MUS_VAL_TRUE, HAWK_T("true"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_306_fields[] = { { HAWK_T("boolean"), &spec_node_307 } };
static const value_t spec_node_306 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_306_fields, HAWK_NULL, 1 };
static const value_t spec_node_308 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_310 = { HAWK_MUS_VAL_FALSE, HAWK_T("false"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_309_fields[] = { { HAWK_T("boolean"), &spec_node_310 } };
static const value_t spec_node_309 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_309_fields, HAWK_NULL, 1 };
static const value_t spec_node_311 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_313 = { HAWK_MUS_VAL_TRUE, HAWK_T("true"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_312_fields[] = { { HAWK_T("boolean"), &spec_node_313 } };
static const value_t spec_node_312 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_312_fields, HAWK_NULL, 1 };
static const value_t spec_node_314 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_316 = { HAWK_MUS_VAL_NIL, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_315_fields[] = { { HAWK_T("null"), &spec_node_316 } };
static const value_t spec_node_315 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_315_fields, HAWK_NULL, 1 };
static const value_t spec_node_317 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_320 = { HAWK_MUS_VAL_SCALAR, HAWK_T("Joe"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_319_fields[] = { { HAWK_T("name"), &spec_node_320 } };
static const value_t spec_node_319 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_319_fields, HAWK_NULL, 1 };
static const field_t spec_node_318_fields[] = { { HAWK_T("context"), &spec_node_319 } };
static const value_t spec_node_318 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_318_fields, HAWK_NULL, 1 };
static const value_t spec_node_321 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_325 = { HAWK_MUS_VAL_SCALAR, HAWK_T("1"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_324_fields[] = { { HAWK_T("n"), &spec_node_325 } };
static const value_t spec_node_324 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_324_fields, HAWK_NULL, 1 };
static const value_t spec_node_327 = { HAWK_MUS_VAL_SCALAR, HAWK_T("2"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_326_fields[] = { { HAWK_T("n"), &spec_node_327 } };
static const value_t spec_node_326 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_326_fields, HAWK_NULL, 1 };
static const value_t spec_node_329 = { HAWK_MUS_VAL_SCALAR, HAWK_T("3"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_328_fields[] = { { HAWK_T("n"), &spec_node_329 } };
static const value_t spec_node_328 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_328_fields, HAWK_NULL, 1 };
static const value_t* const spec_node_323_items[] = { &spec_node_324, &spec_node_326, &spec_node_328 };
static const value_t spec_node_323 = { HAWK_MUS_VAL_ARR, HAWK_T(""), HAWK_NULL, spec_node_323_items, 3 };
static const field_t spec_node_322_fields[] = { { HAWK_T("list"), &spec_node_323 } };
static const value_t spec_node_322 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_322_fields, HAWK_NULL, 1 };
static const value_t spec_node_330 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_332 = { HAWK_MUS_VAL_ARR, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_331_fields[] = { { HAWK_T("list"), &spec_node_332 } };
static const value_t spec_node_331 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_331_fields, HAWK_NULL, 1 };
static const value_t spec_node_333 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_335 = { HAWK_MUS_VAL_FALSE, HAWK_T("false"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_336 = { HAWK_MUS_VAL_SCALAR, HAWK_T("second"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_334_fields[] = { { HAWK_T("bool"), &spec_node_335 }, { HAWK_T("two"), &spec_node_336 } };
static const value_t spec_node_334 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_334_fields, HAWK_NULL, 2 };
static const value_t spec_node_337 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_339 = { HAWK_MUS_VAL_FALSE, HAWK_T("false"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_338_fields[] = { { HAWK_T("bool"), &spec_node_339 } };
static const value_t spec_node_338 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_338_fields, HAWK_NULL, 1 };
static const value_t spec_node_340 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_342 = { HAWK_MUS_VAL_TRUE, HAWK_T("true"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_341_fields[] = { { HAWK_T("bool"), &spec_node_342 } };
static const value_t spec_node_341 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_341_fields, HAWK_NULL, 1 };
static const value_t spec_node_343 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_344 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_345 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_349 = { HAWK_MUS_VAL_TRUE, HAWK_T("true"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_348_fields[] = { { HAWK_T("c"), &spec_node_349 } };
static const value_t spec_node_348 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_348_fields, HAWK_NULL, 1 };
static const field_t spec_node_347_fields[] = { { HAWK_T("b"), &spec_node_348 } };
static const value_t spec_node_347 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_347_fields, HAWK_NULL, 1 };
static const field_t spec_node_346_fields[] = { { HAWK_T("a"), &spec_node_347 } };
static const value_t spec_node_346 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_346_fields, HAWK_NULL, 1 };
static const value_t spec_node_350 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_354 = { HAWK_MUS_VAL_FALSE, HAWK_T("false"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_353_fields[] = { { HAWK_T("c"), &spec_node_354 } };
static const value_t spec_node_353 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_353_fields, HAWK_NULL, 1 };
static const field_t spec_node_352_fields[] = { { HAWK_T("b"), &spec_node_353 } };
static const value_t spec_node_352 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_352_fields, HAWK_NULL, 1 };
static const field_t spec_node_351_fields[] = { { HAWK_T("a"), &spec_node_352 } };
static const value_t spec_node_351 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_351_fields, HAWK_NULL, 1 };
static const value_t spec_node_355 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_357 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_356_fields[] = { { HAWK_T("a"), &spec_node_357 } };
static const value_t spec_node_356 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_356_fields, HAWK_NULL, 1 };
static const value_t spec_node_358 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_360 = { HAWK_MUS_VAL_FALSE, HAWK_T("false"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_359_fields[] = { { HAWK_T("boolean"), &spec_node_360 } };
static const value_t spec_node_359 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_359_fields, HAWK_NULL, 1 };
static const value_t spec_node_361 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_363 = { HAWK_MUS_VAL_FALSE, HAWK_T("false"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_362_fields[] = { { HAWK_T("boolean"), &spec_node_363 } };
static const value_t spec_node_362 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_362_fields, HAWK_NULL, 1 };
static const value_t spec_node_364 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_366 = { HAWK_MUS_VAL_FALSE, HAWK_T("false"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_365_fields[] = { { HAWK_T("boolean"), &spec_node_366 } };
static const value_t spec_node_365 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_365_fields, HAWK_NULL, 1 };
static const value_t spec_node_367 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_369 = { HAWK_MUS_VAL_FALSE, HAWK_T("false"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_368_fields[] = { { HAWK_T("boolean"), &spec_node_369 } };
static const value_t spec_node_368 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_368_fields, HAWK_NULL, 1 };
static const value_t spec_node_370 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_372 = { HAWK_MUS_VAL_FALSE, HAWK_T("false"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_371_fields[] = { { HAWK_T("boolean"), &spec_node_372 } };
static const value_t spec_node_371 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_371_fields, HAWK_NULL, 1 };
static const value_t spec_node_373 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_375 = { HAWK_MUS_VAL_FALSE, HAWK_T("false"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_374_fields[] = { { HAWK_T("boolean"), &spec_node_375 } };
static const value_t spec_node_374 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_374_fields, HAWK_NULL, 1 };
static const value_t spec_node_376 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_378 = { HAWK_MUS_VAL_FALSE, HAWK_T("false"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_377_fields[] = { { HAWK_T("boolean"), &spec_node_378 } };
static const value_t spec_node_377 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_377_fields, HAWK_NULL, 1 };
static const value_t spec_node_379 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_381 = { HAWK_MUS_VAL_FALSE, HAWK_T("false"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_380_fields[] = { { HAWK_T("boolean"), &spec_node_381 } };
static const value_t spec_node_380 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_380_fields, HAWK_NULL, 1 };
static const value_t spec_node_382 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_384 = { HAWK_MUS_VAL_FALSE, HAWK_T("false"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_383_fields[] = { { HAWK_T("boolean"), &spec_node_384 } };
static const value_t spec_node_383 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_383_fields, HAWK_NULL, 1 };
static const value_t spec_node_385 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_386 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_387 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_388 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_389 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_390 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_391 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_392 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_393 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_394 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_395 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_396 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_397 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_398 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_399 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_400 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_401 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_402 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_403 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_404 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_405 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_406 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_407 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_409 = { HAWK_MUS_VAL_SCALAR, HAWK_T("1"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_410 = { HAWK_MUS_VAL_SCALAR, HAWK_T("2"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_411 = { HAWK_MUS_VAL_SCALAR, HAWK_T("3"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_412 = { HAWK_MUS_VAL_SCALAR, HAWK_T("4"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_408_fields[] = { { HAWK_T("! comment"), &spec_node_409 }, { HAWK_T("! comment "), &spec_node_410 }, { HAWK_T("!comment"), &spec_node_411 }, { HAWK_T("comment"), &spec_node_412 } };
static const value_t spec_node_408 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_408_fields, HAWK_NULL, 4 };
static const value_t spec_node_413 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_414 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_416 = { HAWK_MUS_VAL_SCALAR, HAWK_T("from partial"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_415_fields[] = { { HAWK_T("text"), &spec_node_416 } };
static const value_t spec_node_415 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_415_fields, HAWK_NULL, 1 };
static const value_t spec_node_417 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_418 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_420 = { HAWK_MUS_VAL_SCALAR, HAWK_T("content"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_419_fields[] = { { HAWK_T("text"), &spec_node_420 } };
static const value_t spec_node_419 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_419_fields, HAWK_NULL, 1 };
static const value_t spec_node_422 = { HAWK_MUS_VAL_SCALAR, HAWK_T("*{{text}}*"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_421_fields[] = { { HAWK_T("partial"), &spec_node_422 } };
static const value_t spec_node_421 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_421_fields, HAWK_NULL, 1 };
static const value_t spec_node_424 = { HAWK_MUS_VAL_SCALAR, HAWK_T("X"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_427 = { HAWK_MUS_VAL_SCALAR, HAWK_T("Y"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_428 = { HAWK_MUS_VAL_ARR, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_426_fields[] = { { HAWK_T("content"), &spec_node_427 }, { HAWK_T("nodes"), &spec_node_428 } };
static const value_t spec_node_426 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_426_fields, HAWK_NULL, 2 };
static const value_t* const spec_node_425_items[] = { &spec_node_426 };
static const value_t spec_node_425 = { HAWK_MUS_VAL_ARR, HAWK_T(""), HAWK_NULL, spec_node_425_items, 1 };
static const field_t spec_node_423_fields[] = { { HAWK_T("content"), &spec_node_424 }, { HAWK_T("nodes"), &spec_node_425 } };
static const value_t spec_node_423 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_423_fields, HAWK_NULL, 2 };
static const value_t spec_node_430 = { HAWK_MUS_VAL_SCALAR, HAWK_T("{{content}}<{{#nodes}}{{>node}}{{/nodes}}>"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_429_fields[] = { { HAWK_T("node"), &spec_node_430 } };
static const value_t spec_node_429 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_429_fields, HAWK_NULL, 1 };
static const value_t spec_node_432 = { HAWK_MUS_VAL_SCALAR, HAWK_T("hello"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_433 = { HAWK_MUS_VAL_SCALAR, HAWK_T("world"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_431_fields[] = { { HAWK_T("a"), &spec_node_432 }, { HAWK_T("b"), &spec_node_433 } };
static const value_t spec_node_431 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_431_fields, HAWK_NULL, 2 };
static const value_t spec_node_435 = { HAWK_MUS_VAL_SCALAR, HAWK_T("*{{a}} {{>inner}}*"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_436 = { HAWK_MUS_VAL_SCALAR, HAWK_T("{{b}}!"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_434_fields[] = { { HAWK_T("outer"), &spec_node_435 }, { HAWK_T("inner"), &spec_node_436 } };
static const value_t spec_node_434 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_434_fields, HAWK_NULL, 2 };
static const value_t spec_node_437 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_439 = { HAWK_MUS_VAL_SCALAR, HAWK_T("\t|\t"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_438_fields[] = { { HAWK_T("partial"), &spec_node_439 } };
static const value_t spec_node_438 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_438_fields, HAWK_NULL, 1 };
static const value_t spec_node_441 = { HAWK_MUS_VAL_SCALAR, HAWK_T("|"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_440_fields[] = { { HAWK_T("data"), &spec_node_441 } };
static const value_t spec_node_440 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_440_fields, HAWK_NULL, 1 };
static const value_t spec_node_443 = { HAWK_MUS_VAL_SCALAR, HAWK_T(">\n>"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_442_fields[] = { { HAWK_T("partial"), &spec_node_443 } };
static const value_t spec_node_442 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_442_fields, HAWK_NULL, 1 };
static const value_t spec_node_444 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_446 = { HAWK_MUS_VAL_SCALAR, HAWK_T(">"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_445_fields[] = { { HAWK_T("partial"), &spec_node_446 } };
static const value_t spec_node_445 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_445_fields, HAWK_NULL, 1 };
static const value_t spec_node_447 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_449 = { HAWK_MUS_VAL_SCALAR, HAWK_T(">\n>"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_448_fields[] = { { HAWK_T("partial"), &spec_node_449 } };
static const value_t spec_node_448 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_448_fields, HAWK_NULL, 1 };
static const value_t spec_node_450 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_452 = { HAWK_MUS_VAL_SCALAR, HAWK_T(">\n>"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_451_fields[] = { { HAWK_T("partial"), &spec_node_452 } };
static const value_t spec_node_451 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_451_fields, HAWK_NULL, 1 };
static const value_t spec_node_454 = { HAWK_MUS_VAL_SCALAR, HAWK_T("<\n->"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_453_fields[] = { { HAWK_T("content"), &spec_node_454 } };
static const value_t spec_node_453 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_453_fields, HAWK_NULL, 1 };
static const value_t spec_node_456 = { HAWK_MUS_VAL_SCALAR, HAWK_T("|\n{{{content}}}\n|\n"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_455_fields[] = { { HAWK_T("partial"), &spec_node_456 } };
static const value_t spec_node_455 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_455_fields, HAWK_NULL, 1 };
static const value_t spec_node_458 = { HAWK_MUS_VAL_TRUE, HAWK_T("true"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_457_fields[] = { { HAWK_T("boolean"), &spec_node_458 } };
static const value_t spec_node_457 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_457_fields, HAWK_NULL, 1 };
static const value_t spec_node_460 = { HAWK_MUS_VAL_SCALAR, HAWK_T("[]"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_459_fields[] = { { HAWK_T("partial"), &spec_node_460 } };
static const value_t spec_node_459 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_459_fields, HAWK_NULL, 1 };
static const value_t spec_node_462 = { HAWK_MUS_VAL_SCALAR, HAWK_T("Hey!"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_461_fields[] = { { HAWK_T("text"), &spec_node_462 } };
static const value_t spec_node_461 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_461_fields, HAWK_NULL, 1 };
static const value_t spec_node_463 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_465 = { HAWK_MUS_VAL_SCALAR, HAWK_T("It worked!"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_464_fields[] = { { HAWK_T("text"), &spec_node_465 } };
static const value_t spec_node_464 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_464_fields, HAWK_NULL, 1 };
static const value_t spec_node_466 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_468 = { HAWK_MUS_VAL_TRUE, HAWK_T("true"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_469 = { HAWK_MUS_VAL_SCALAR, HAWK_T("I got interpolated."), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_467_fields[] = { { HAWK_T("section"), &spec_node_468 }, { HAWK_T("data"), &spec_node_469 } };
static const value_t spec_node_467 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_467_fields, HAWK_NULL, 2 };
static const value_t spec_node_470 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_472 = { HAWK_MUS_VAL_FALSE, HAWK_T("false"), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_473 = { HAWK_MUS_VAL_SCALAR, HAWK_T("I got interpolated."), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_471_fields[] = { { HAWK_T("section"), &spec_node_472 }, { HAWK_T("data"), &spec_node_473 } };
static const value_t spec_node_471 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_471_fields, HAWK_NULL, 2 };
static const value_t spec_node_474 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_476 = { HAWK_MUS_VAL_SCALAR, HAWK_T("yes"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_475_fields[] = { { HAWK_T("value"), &spec_node_476 } };
static const value_t spec_node_475 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_475_fields, HAWK_NULL, 1 };
static const value_t spec_node_478 = { HAWK_MUS_VAL_SCALAR, HAWK_T(".{{value}}."), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_477_fields[] = { { HAWK_T("include"), &spec_node_478 } };
static const value_t spec_node_477 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_477_fields, HAWK_NULL, 1 };
static const value_t spec_node_480 = { HAWK_MUS_VAL_SCALAR, HAWK_T("yes"), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_479_fields[] = { { HAWK_T("value"), &spec_node_480 } };
static const value_t spec_node_479 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_479_fields, HAWK_NULL, 1 };
static const value_t spec_node_482 = { HAWK_MUS_VAL_SCALAR, HAWK_T(".{{value}}. {{= | | =}} .|value|."), HAWK_NULL, HAWK_NULL, 0 };
static const field_t spec_node_481_fields[] = { { HAWK_T("include"), &spec_node_482 } };
static const value_t spec_node_481 = { HAWK_MUS_VAL_MAP, HAWK_T(""), spec_node_481_fields, HAWK_NULL, 1 };
static const value_t spec_node_483 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_484 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_485 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_486 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_487 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_488 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_489 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_490 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_491 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_492 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_493 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_494 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_495 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_496 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_497 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const value_t spec_node_498 = { HAWK_MUS_VAL_MAP, HAWK_T(""), HAWK_NULL, HAWK_NULL, 0 };
static const spec_t specs[] = {
	{ "interpolation/No Interpolation", HAWK_T("Hello from {Mustache}!\n"), &spec_node_1, &spec_node_2, HAWK_T("Hello from {Mustache}!\n") },
	{ "interpolation/Basic Interpolation", HAWK_T("Hello, {{subject}}!\n"), &spec_node_3, &spec_node_5, HAWK_T("Hello, world!\n") },
	{ "interpolation/HTML Escaping", HAWK_T("These characters should be HTML escaped: {{forbidden}}\n"), &spec_node_6, &spec_node_8, HAWK_T("These characters should be HTML escaped: &amp; &quot; &lt; &gt;\n") },
	{ "interpolation/Triple Mustache", HAWK_T("These characters should not be HTML escaped: {{{forbidden}}}\n"), &spec_node_9, &spec_node_11, HAWK_T("These characters should not be HTML escaped: & \" < >\n") },
	{ "interpolation/Ampersand", HAWK_T("These characters should not be HTML escaped: {{&forbidden}}\n"), &spec_node_12, &spec_node_14, HAWK_T("These characters should not be HTML escaped: & \" < >\n") },
	{ "interpolation/Basic Integer Interpolation", HAWK_T("\"{{mph}} miles an hour!\""), &spec_node_15, &spec_node_17, HAWK_T("\"85 miles an hour!\"") },
	{ "interpolation/Triple Mustache Integer Interpolation", HAWK_T("\"{{{mph}}} miles an hour!\""), &spec_node_18, &spec_node_20, HAWK_T("\"85 miles an hour!\"") },
	{ "interpolation/Ampersand Integer Interpolation", HAWK_T("\"{{&mph}} miles an hour!\""), &spec_node_21, &spec_node_23, HAWK_T("\"85 miles an hour!\"") },
	{ "interpolation/Basic Decimal Interpolation", HAWK_T("\"{{power}} jiggawatts!\""), &spec_node_24, &spec_node_26, HAWK_T("\"1.21 jiggawatts!\"") },
	{ "interpolation/Triple Mustache Decimal Interpolation", HAWK_T("\"{{{power}}} jiggawatts!\""), &spec_node_27, &spec_node_29, HAWK_T("\"1.21 jiggawatts!\"") },
	{ "interpolation/Ampersand Decimal Interpolation", HAWK_T("\"{{&power}} jiggawatts!\""), &spec_node_30, &spec_node_32, HAWK_T("\"1.21 jiggawatts!\"") },
	{ "interpolation/Basic Null Interpolation", HAWK_T("I ({{cannot}}) be seen!"), &spec_node_33, &spec_node_35, HAWK_T("I () be seen!") },
	{ "interpolation/Triple Mustache Null Interpolation", HAWK_T("I ({{{cannot}}}) be seen!"), &spec_node_36, &spec_node_38, HAWK_T("I () be seen!") },
	{ "interpolation/Ampersand Null Interpolation", HAWK_T("I ({{&cannot}}) be seen!"), &spec_node_39, &spec_node_41, HAWK_T("I () be seen!") },
	{ "interpolation/Basic Context Miss Interpolation", HAWK_T("I ({{cannot}}) be seen!"), &spec_node_42, &spec_node_43, HAWK_T("I () be seen!") },
	{ "interpolation/Triple Mustache Context Miss Interpolation", HAWK_T("I ({{{cannot}}}) be seen!"), &spec_node_44, &spec_node_45, HAWK_T("I () be seen!") },
	{ "interpolation/Ampersand Context Miss Interpolation", HAWK_T("I ({{&cannot}}) be seen!"), &spec_node_46, &spec_node_47, HAWK_T("I () be seen!") },
	{ "interpolation/Dotted Names - Basic Interpolation", HAWK_T("\"{{person.name}}\" == \"{{#person}}{{name}}{{/person}}\""), &spec_node_48, &spec_node_51, HAWK_T("\"Joe\" == \"Joe\"") },
	{ "interpolation/Dotted Names - Triple Mustache Interpolation", HAWK_T("\"{{{person.name}}}\" == \"{{#person}}{{{name}}}{{/person}}\""), &spec_node_52, &spec_node_55, HAWK_T("\"Joe\" == \"Joe\"") },
	{ "interpolation/Dotted Names - Ampersand Interpolation", HAWK_T("\"{{&person.name}}\" == \"{{#person}}{{&name}}{{/person}}\""), &spec_node_56, &spec_node_59, HAWK_T("\"Joe\" == \"Joe\"") },
	{ "interpolation/Dotted Names - Arbitrary Depth", HAWK_T("\"{{a.b.c.d.e.name}}\" == \"Phil\""), &spec_node_60, &spec_node_67, HAWK_T("\"Phil\" == \"Phil\"") },
	{ "interpolation/Dotted Names - Broken Chains", HAWK_T("\"{{a.b.c}}\" == \"\""), &spec_node_68, &spec_node_70, HAWK_T("\"\" == \"\"") },
	{ "interpolation/Dotted Names - Broken Chain Resolution", HAWK_T("\"{{a.b.c.name}}\" == \"\""), &spec_node_71, &spec_node_76, HAWK_T("\"\" == \"\"") },
	{ "interpolation/Dotted Names - Initial Resolution", HAWK_T("\"{{#a}}{{b.c.d.e.name}}{{/a}}\" == \"Phil\""), &spec_node_77, &spec_node_89, HAWK_T("\"Phil\" == \"Phil\"") },
	{ "interpolation/Dotted Names - Context Precedence", HAWK_T("{{#a}}{{b.c}}{{/a}}"), &spec_node_90, &spec_node_95, HAWK_T("") },
	{ "interpolation/Implicit Iterators - Basic Interpolation", HAWK_T("Hello, {{.}}!\n"), &spec_node_96, &spec_node_97, HAWK_T("Hello, world!\n") },
	{ "interpolation/Implicit Iterators - HTML Escaping", HAWK_T("These characters should be HTML escaped: {{.}}\n"), &spec_node_98, &spec_node_99, HAWK_T("These characters should be HTML escaped: &amp; &quot; &lt; &gt;\n") },
	{ "interpolation/Implicit Iterators - Triple Mustache", HAWK_T("These characters should not be HTML escaped: {{{.}}}\n"), &spec_node_100, &spec_node_101, HAWK_T("These characters should not be HTML escaped: & \" < >\n") },
	{ "interpolation/Implicit Iterators - Ampersand", HAWK_T("These characters should not be HTML escaped: {{&.}}\n"), &spec_node_102, &spec_node_103, HAWK_T("These characters should not be HTML escaped: & \" < >\n") },
	{ "interpolation/Implicit Iterators - Basic Integer Interpolation", HAWK_T("\"{{.}} miles an hour!\""), &spec_node_104, &spec_node_105, HAWK_T("\"85 miles an hour!\"") },
	{ "interpolation/Interpolation - Surrounding Whitespace", HAWK_T("| {{string}} |"), &spec_node_106, &spec_node_108, HAWK_T("| --- |") },
	{ "interpolation/Triple Mustache - Surrounding Whitespace", HAWK_T("| {{{string}}} |"), &spec_node_109, &spec_node_111, HAWK_T("| --- |") },
	{ "interpolation/Ampersand - Surrounding Whitespace", HAWK_T("| {{&string}} |"), &spec_node_112, &spec_node_114, HAWK_T("| --- |") },
	{ "interpolation/Interpolation - Standalone", HAWK_T("  {{string}}\n"), &spec_node_115, &spec_node_117, HAWK_T("  ---\n") },
	{ "interpolation/Triple Mustache - Standalone", HAWK_T("  {{{string}}}\n"), &spec_node_118, &spec_node_120, HAWK_T("  ---\n") },
	{ "interpolation/Ampersand - Standalone", HAWK_T("  {{&string}}\n"), &spec_node_121, &spec_node_123, HAWK_T("  ---\n") },
	{ "interpolation/Interpolation With Padding", HAWK_T("|{{ string }}|"), &spec_node_124, &spec_node_126, HAWK_T("|---|") },
	{ "interpolation/Triple Mustache With Padding", HAWK_T("|{{{ string }}}|"), &spec_node_127, &spec_node_129, HAWK_T("|---|") },
	{ "interpolation/Ampersand With Padding", HAWK_T("|{{& string }}|"), &spec_node_130, &spec_node_132, HAWK_T("|---|") },
	{ "sections/Truthy", HAWK_T("\"{{#boolean}}This should be rendered.{{/boolean}}\""), &spec_node_133, &spec_node_135, HAWK_T("\"This should be rendered.\"") },
	{ "sections/Falsey", HAWK_T("\"{{#boolean}}This should not be rendered.{{/boolean}}\""), &spec_node_136, &spec_node_138, HAWK_T("\"\"") },
	{ "sections/Null is falsey", HAWK_T("\"{{#null}}This should not be rendered.{{/null}}\""), &spec_node_139, &spec_node_141, HAWK_T("\"\"") },
	{ "sections/Context", HAWK_T("\"{{#context}}Hi {{name}}.{{/context}}\""), &spec_node_142, &spec_node_145, HAWK_T("\"Hi Joe.\"") },
	{ "sections/Parent contexts", HAWK_T("\"{{#sec}}{{a}}, {{b}}, {{c.d}}{{/sec}}\""), &spec_node_146, &spec_node_153, HAWK_T("\"foo, bar, baz\"") },
	{ "sections/Variable test", HAWK_T("\"{{#foo}}{{.}} is {{foo}}{{/foo}}\""), &spec_node_154, &spec_node_156, HAWK_T("\"bar is bar\"") },
	{ "sections/List Contexts", HAWK_T("{{#tops}}{{#middles}}{{tname.lower}}{{mname}}.{{#bottoms}}{{tname.upper}}{{mname}}{{bname}}.{{/bottoms}}{{/middles}}{{/tops}}"), &spec_node_157, &spec_node_171, HAWK_T("a1.A1x.A1y.") },
	{ "sections/Deeply Nested Contexts", HAWK_T("{{#a}}\n{{one}}\n{{#b}}\n{{one}}{{two}}{{one}}\n{{#c}}\n{{one}}{{two}}{{three}}{{two}}{{one}}\n{{#d}}\n{{one}}{{two}}{{three}}{{four}}{{three}}{{two}}{{one}}\n{{#five}}\n{{one}}{{two}}{{three}}{{four}}{{five}}{{four}}{{three}}{{two}}{{one}}\n{{one}}{{two}}{{three}}{{four}}{{.}}6{{.}}{{four}}{{three}}{{two}}{{one}}\n{{one}}{{two}}{{three}}{{four}}{{five}}{{four}}{{three}}{{two}}{{one}}\n{{/five}}\n{{one}}{{two}}{{three}}{{four}}{{three}}{{two}}{{one}}\n{{/d}}\n{{one}}{{two}}{{three}}{{two}}{{one}}\n{{/c}}\n{{one}}{{two}}{{one}}\n{{/b}}\n{{one}}\n{{/a}}\n"), &spec_node_172, &spec_node_182, HAWK_T("1\n121\n12321\n1234321\n123454321\n12345654321\n123454321\n1234321\n12321\n121\n1\n") },
	{ "sections/List", HAWK_T("\"{{#list}}{{item}}{{/list}}\""), &spec_node_183, &spec_node_191, HAWK_T("\"123\"") },
	{ "sections/Empty List", HAWK_T("\"{{#list}}Yay lists!{{/list}}\""), &spec_node_192, &spec_node_194, HAWK_T("\"\"") },
	{ "sections/Doubled", HAWK_T("{{#bool}}\n* first\n{{/bool}}\n* {{two}}\n{{#bool}}\n* third\n{{/bool}}\n"), &spec_node_195, &spec_node_198, HAWK_T("* first\n* second\n* third\n") },
	{ "sections/Nested (Truthy)", HAWK_T("| A {{#bool}}B {{#bool}}C{{/bool}} D{{/bool}} E |"), &spec_node_199, &spec_node_201, HAWK_T("| A B C D E |") },
	{ "sections/Nested (Falsey)", HAWK_T("| A {{#bool}}B {{#bool}}C{{/bool}} D{{/bool}} E |"), &spec_node_202, &spec_node_204, HAWK_T("| A  E |") },
	{ "sections/Context Misses", HAWK_T("[{{#missing}}Found key 'missing'!{{/missing}}]"), &spec_node_205, &spec_node_206, HAWK_T("[]") },
	{ "sections/Implicit Iterator - String", HAWK_T("\"{{#list}}({{.}}){{/list}}\""), &spec_node_207, &spec_node_214, HAWK_T("\"(a)(b)(c)(d)(e)\"") },
	{ "sections/Implicit Iterator - Integer", HAWK_T("\"{{#list}}({{.}}){{/list}}\""), &spec_node_215, &spec_node_222, HAWK_T("\"(1)(2)(3)(4)(5)\"") },
	{ "sections/Implicit Iterator - Decimal", HAWK_T("\"{{#list}}({{.}}){{/list}}\""), &spec_node_223, &spec_node_230, HAWK_T("\"(1.1)(2.2)(3.3)(4.4)(5.5)\"") },
	{ "sections/Implicit Iterator - Array", HAWK_T("\"{{#list}}({{#.}}{{.}}{{/.}}){{/list}}\""), &spec_node_231, &spec_node_241, HAWK_T("\"(123)(abc)\"") },
	{ "sections/Implicit Iterator - HTML Escaping", HAWK_T("\"{{#list}}({{.}}){{/list}}\""), &spec_node_242, &spec_node_248, HAWK_T("\"(&amp;)(&quot;)(&lt;)(&gt;)\"") },
	{ "sections/Implicit Iterator - Triple mustache", HAWK_T("\"{{#list}}({{{.}}}){{/list}}\""), &spec_node_249, &spec_node_255, HAWK_T("\"(&)(\")(<)(>)\"") },
	{ "sections/Implicit Iterator - Ampersand", HAWK_T("\"{{#list}}({{&.}}){{/list}}\""), &spec_node_256, &spec_node_262, HAWK_T("\"(&)(\")(<)(>)\"") },
	{ "sections/Implicit Iterator - Root-level", HAWK_T("\"{{#.}}({{value}}){{/.}}\""), &spec_node_263, &spec_node_268, HAWK_T("\"(a)(b)\"") },
	{ "sections/Dotted Names - Truthy", HAWK_T("\"{{#a.b.c}}Here{{/a.b.c}}\" == \"Here\""), &spec_node_269, &spec_node_273, HAWK_T("\"Here\" == \"Here\"") },
	{ "sections/Dotted Names - Falsey", HAWK_T("\"{{#a.b.c}}Here{{/a.b.c}}\" == \"\""), &spec_node_274, &spec_node_278, HAWK_T("\"\" == \"\"") },
	{ "sections/Dotted Names - Broken Chains", HAWK_T("\"{{#a.b.c}}Here{{/a.b.c}}\" == \"\""), &spec_node_279, &spec_node_281, HAWK_T("\"\" == \"\"") },
	{ "sections/Surrounding Whitespace", HAWK_T(" | {{#boolean}}\t|\t{{/boolean}} | \n"), &spec_node_282, &spec_node_284, HAWK_T(" | \t|\t | \n") },
	{ "sections/Internal Whitespace", HAWK_T(" | {{#boolean}} {{! Important Whitespace }}\n {{/boolean}} | \n"), &spec_node_285, &spec_node_287, HAWK_T(" |  \n  | \n") },
	{ "sections/Indented Inline Sections", HAWK_T(" {{#boolean}}YES{{/boolean}}\n {{#boolean}}GOOD{{/boolean}}\n"), &spec_node_288, &spec_node_290, HAWK_T(" YES\n GOOD\n") },
	{ "sections/Standalone Lines", HAWK_T("| This Is\n{{#boolean}}\n|\n{{/boolean}}\n| A Line\n"), &spec_node_291, &spec_node_293, HAWK_T("| This Is\n|\n| A Line\n") },
	{ "sections/Indented Standalone Lines", HAWK_T("| This Is\n  {{#boolean}}\n|\n  {{/boolean}}\n| A Line\n"), &spec_node_294, &spec_node_296, HAWK_T("| This Is\n|\n| A Line\n") },
	{ "sections/Standalone Line Endings", HAWK_T("|\r\n{{#boolean}}\r\n{{/boolean}}\r\n|"), &spec_node_297, &spec_node_299, HAWK_T("|\r\n|") },
	{ "sections/Standalone Without Previous Line", HAWK_T("  {{#boolean}}\n#{{/boolean}}\n/"), &spec_node_300, &spec_node_302, HAWK_T("#\n/") },
	{ "sections/Standalone Without Newline", HAWK_T("#{{#boolean}}\n/\n  {{/boolean}}"), &spec_node_303, &spec_node_305, HAWK_T("#\n/\n") },
	{ "sections/Padding", HAWK_T("|{{# boolean }}={{/ boolean }}|"), &spec_node_306, &spec_node_308, HAWK_T("|=|") },
	{ "inverted/Falsey", HAWK_T("\"{{^boolean}}This should be rendered.{{/boolean}}\""), &spec_node_309, &spec_node_311, HAWK_T("\"This should be rendered.\"") },
	{ "inverted/Truthy", HAWK_T("\"{{^boolean}}This should not be rendered.{{/boolean}}\""), &spec_node_312, &spec_node_314, HAWK_T("\"\"") },
	{ "inverted/Null is falsey", HAWK_T("\"{{^null}}This should be rendered.{{/null}}\""), &spec_node_315, &spec_node_317, HAWK_T("\"This should be rendered.\"") },
	{ "inverted/Context", HAWK_T("\"{{^context}}Hi {{name}}.{{/context}}\""), &spec_node_318, &spec_node_321, HAWK_T("\"\"") },
	{ "inverted/List", HAWK_T("\"{{^list}}{{n}}{{/list}}\""), &spec_node_322, &spec_node_330, HAWK_T("\"\"") },
	{ "inverted/Empty List", HAWK_T("\"{{^list}}Yay lists!{{/list}}\""), &spec_node_331, &spec_node_333, HAWK_T("\"Yay lists!\"") },
	{ "inverted/Doubled", HAWK_T("{{^bool}}\n* first\n{{/bool}}\n* {{two}}\n{{^bool}}\n* third\n{{/bool}}\n"), &spec_node_334, &spec_node_337, HAWK_T("* first\n* second\n* third\n") },
	{ "inverted/Nested (Falsey)", HAWK_T("| A {{^bool}}B {{^bool}}C{{/bool}} D{{/bool}} E |"), &spec_node_338, &spec_node_340, HAWK_T("| A B C D E |") },
	{ "inverted/Nested (Truthy)", HAWK_T("| A {{^bool}}B {{^bool}}C{{/bool}} D{{/bool}} E |"), &spec_node_341, &spec_node_343, HAWK_T("| A  E |") },
	{ "inverted/Context Misses", HAWK_T("[{{^missing}}Cannot find key 'missing'!{{/missing}}]"), &spec_node_344, &spec_node_345, HAWK_T("[Cannot find key 'missing'!]") },
	{ "inverted/Dotted Names - Truthy", HAWK_T("\"{{^a.b.c}}Not Here{{/a.b.c}}\" == \"\""), &spec_node_346, &spec_node_350, HAWK_T("\"\" == \"\"") },
	{ "inverted/Dotted Names - Falsey", HAWK_T("\"{{^a.b.c}}Not Here{{/a.b.c}}\" == \"Not Here\""), &spec_node_351, &spec_node_355, HAWK_T("\"Not Here\" == \"Not Here\"") },
	{ "inverted/Dotted Names - Broken Chains", HAWK_T("\"{{^a.b.c}}Not Here{{/a.b.c}}\" == \"Not Here\""), &spec_node_356, &spec_node_358, HAWK_T("\"Not Here\" == \"Not Here\"") },
	{ "inverted/Surrounding Whitespace", HAWK_T(" | {{^boolean}}\t|\t{{/boolean}} | \n"), &spec_node_359, &spec_node_361, HAWK_T(" | \t|\t | \n") },
	{ "inverted/Internal Whitespace", HAWK_T(" | {{^boolean}} {{! Important Whitespace }}\n {{/boolean}} | \n"), &spec_node_362, &spec_node_364, HAWK_T(" |  \n  | \n") },
	{ "inverted/Indented Inline Sections", HAWK_T(" {{^boolean}}NO{{/boolean}}\n {{^boolean}}WAY{{/boolean}}\n"), &spec_node_365, &spec_node_367, HAWK_T(" NO\n WAY\n") },
	{ "inverted/Standalone Lines", HAWK_T("| This Is\n{{^boolean}}\n|\n{{/boolean}}\n| A Line\n"), &spec_node_368, &spec_node_370, HAWK_T("| This Is\n|\n| A Line\n") },
	{ "inverted/Standalone Indented Lines", HAWK_T("| This Is\n  {{^boolean}}\n|\n  {{/boolean}}\n| A Line\n"), &spec_node_371, &spec_node_373, HAWK_T("| This Is\n|\n| A Line\n") },
	{ "inverted/Standalone Line Endings", HAWK_T("|\r\n{{^boolean}}\r\n{{/boolean}}\r\n|"), &spec_node_374, &spec_node_376, HAWK_T("|\r\n|") },
	{ "inverted/Standalone Without Previous Line", HAWK_T("  {{^boolean}}\n^{{/boolean}}\n/"), &spec_node_377, &spec_node_379, HAWK_T("^\n/") },
	{ "inverted/Standalone Without Newline", HAWK_T("^{{^boolean}}\n/\n  {{/boolean}}"), &spec_node_380, &spec_node_382, HAWK_T("^\n/\n") },
	{ "inverted/Padding", HAWK_T("|{{^ boolean }}={{/ boolean }}|"), &spec_node_383, &spec_node_385, HAWK_T("|=|") },
	{ "comments/Inline", HAWK_T("12345{{! Comment Block! }}67890"), &spec_node_386, &spec_node_387, HAWK_T("1234567890") },
	{ "comments/Multiline", HAWK_T("12345{{!\n  This is a\n  multi-line comment...\n}}67890\n"), &spec_node_388, &spec_node_389, HAWK_T("1234567890\n") },
	{ "comments/Standalone", HAWK_T("Begin.\n{{! Comment Block! }}\nEnd.\n"), &spec_node_390, &spec_node_391, HAWK_T("Begin.\nEnd.\n") },
	{ "comments/Indented Standalone", HAWK_T("Begin.\n  {{! Indented Comment Block! }}\nEnd.\n"), &spec_node_392, &spec_node_393, HAWK_T("Begin.\nEnd.\n") },
	{ "comments/Standalone Line Endings", HAWK_T("|\r\n{{! Standalone Comment }}\r\n|"), &spec_node_394, &spec_node_395, HAWK_T("|\r\n|") },
	{ "comments/Standalone Without Previous Line", HAWK_T("  {{! I'm Still Standalone }}\n!"), &spec_node_396, &spec_node_397, HAWK_T("!") },
	{ "comments/Standalone Without Newline", HAWK_T("!\n  {{! I'm Still Standalone }}"), &spec_node_398, &spec_node_399, HAWK_T("!\n") },
	{ "comments/Multiline Standalone", HAWK_T("Begin.\n{{!\nSomething's going on here...\n}}\nEnd.\n"), &spec_node_400, &spec_node_401, HAWK_T("Begin.\nEnd.\n") },
	{ "comments/Indented Multiline Standalone", HAWK_T("Begin.\n  {{!\n    Something's going on here...\n  }}\nEnd.\n"), &spec_node_402, &spec_node_403, HAWK_T("Begin.\nEnd.\n") },
	{ "comments/Indented Inline", HAWK_T("  12 {{! 34 }}\n"), &spec_node_404, &spec_node_405, HAWK_T("  12 \n") },
	{ "comments/Surrounding Whitespace", HAWK_T("12345 {{! Comment Block! }} 67890"), &spec_node_406, &spec_node_407, HAWK_T("12345  67890") },
	{ "comments/Variable Name Collision", HAWK_T("comments never show: >{{! comment }}<"), &spec_node_408, &spec_node_413, HAWK_T("comments never show: ><") },
	{ "partials/Basic Behavior", HAWK_T("\"{{>text}}\""), &spec_node_414, &spec_node_415, HAWK_T("\"from partial\"") },
	{ "partials/Failed Lookup", HAWK_T("\"{{>text}}\""), &spec_node_417, &spec_node_418, HAWK_T("\"\"") },
	{ "partials/Context", HAWK_T("\"{{>partial}}\""), &spec_node_419, &spec_node_421, HAWK_T("\"*content*\"") },
	{ "partials/Recursion", HAWK_T("{{>node}}"), &spec_node_423, &spec_node_429, HAWK_T("X<Y<>>") },
	{ "partials/Nested", HAWK_T("{{>outer}}"), &spec_node_431, &spec_node_434, HAWK_T("*hello world!*") },
	{ "partials/Surrounding Whitespace", HAWK_T("| {{>partial}} |"), &spec_node_437, &spec_node_438, HAWK_T("| \t|\t |") },
	{ "partials/Inline Indentation", HAWK_T("  {{data}}  {{> partial}}\n"), &spec_node_440, &spec_node_442, HAWK_T("  |  >\n>\n") },
	{ "partials/Standalone Line Endings", HAWK_T("|\r\n{{>partial}}\r\n|"), &spec_node_444, &spec_node_445, HAWK_T("|\r\n>|") },
	{ "partials/Standalone Without Previous Line", HAWK_T("  {{>partial}}\n>"), &spec_node_447, &spec_node_448, HAWK_T("  >\n  >>") },
	{ "partials/Standalone Without Newline", HAWK_T(">\n  {{>partial}}"), &spec_node_450, &spec_node_451, HAWK_T(">\n  >\n  >") },
	{ "partials/Standalone Indentation", HAWK_T("\\\n {{>partial}}\n/\n"), &spec_node_453, &spec_node_455, HAWK_T("\\\n |\n <\n->\n |\n/\n") },
	{ "partials/Padding Whitespace", HAWK_T("|{{> partial }}|"), &spec_node_457, &spec_node_459, HAWK_T("|[]|") },
	{ "delimiters/Pair Behavior", HAWK_T("{{=<% %>=}}(<%text%>)"), &spec_node_461, &spec_node_463, HAWK_T("(Hey!)") },
	{ "delimiters/Special Characters", HAWK_T("({{=[ ]=}}[text])"), &spec_node_464, &spec_node_466, HAWK_T("(It worked!)") },
	{ "delimiters/Sections", HAWK_T("[\n{{#section}}\n  {{data}}\n  |data|\n{{/section}}\n\n{{= | | =}}\n|#section|\n  {{data}}\n  |data|\n|/section|\n]\n"), &spec_node_467, &spec_node_470, HAWK_T("[\n  I got interpolated.\n  |data|\n\n  {{data}}\n  I got interpolated.\n]\n") },
	{ "delimiters/Inverted Sections", HAWK_T("[\n{{^section}}\n  {{data}}\n  |data|\n{{/section}}\n\n{{= | | =}}\n|^section|\n  {{data}}\n  |data|\n|/section|\n]\n"), &spec_node_471, &spec_node_474, HAWK_T("[\n  I got interpolated.\n  |data|\n\n  {{data}}\n  I got interpolated.\n]\n") },
	{ "delimiters/Partial Inheritence", HAWK_T("[ {{>include}} ]\n{{= | | =}}\n[ |>include| ]\n"), &spec_node_475, &spec_node_477, HAWK_T("[ .yes. ]\n[ .yes. ]\n") },
	{ "delimiters/Post-Partial Behavior", HAWK_T("[ {{>include}} ]\n[ .{{value}}.  .|value|. ]\n"), &spec_node_479, &spec_node_481, HAWK_T("[ .yes.  .yes. ]\n[ .yes.  .|value|. ]\n") },
	{ "delimiters/Surrounding Whitespace", HAWK_T("| {{=@ @=}} |"), &spec_node_483, &spec_node_484, HAWK_T("|  |") },
	{ "delimiters/Outlying Whitespace (Inline)", HAWK_T(" | {{=@ @=}}\n"), &spec_node_485, &spec_node_486, HAWK_T(" | \n") },
	{ "delimiters/Standalone Tag", HAWK_T("Begin.\n{{=@ @=}}\nEnd.\n"), &spec_node_487, &spec_node_488, HAWK_T("Begin.\nEnd.\n") },
	{ "delimiters/Indented Standalone Tag", HAWK_T("Begin.\n  {{=@ @=}}\nEnd.\n"), &spec_node_489, &spec_node_490, HAWK_T("Begin.\nEnd.\n") },
	{ "delimiters/Standalone Line Endings", HAWK_T("|\r\n{{= @ @ =}}\r\n|"), &spec_node_491, &spec_node_492, HAWK_T("|\r\n|") },
	{ "delimiters/Standalone Without Previous Line", HAWK_T("  {{=@ @=}}\n="), &spec_node_493, &spec_node_494, HAWK_T("=") },
	{ "delimiters/Standalone Without Newline", HAWK_T("=\n  {{=@ @=}}"), &spec_node_495, &spec_node_496, HAWK_T("=\n") },
	{ "delimiters/Pair with Padding", HAWK_T("|{{= @   @ =}}|"), &spec_node_497, &spec_node_498, HAWK_T("||") }
};
