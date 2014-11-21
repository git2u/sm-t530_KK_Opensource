# Copyright 2013 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Compile time controlled V8 features.

{
  'variables': {
    'v8_compress_startup_data%': 'off',

    'v8_enable_debugger_support%': 1,

    'v8_enable_disassembler%': 0,

    'v8_enable_gdbjit%': 0,

    'v8_object_print%': 0,

    'v8_enable_verify_heap%': 0,

    'v8_use_snapshot%': 'true',

    # With post mortem support enabled, metadata is embedded into libv8 that
    # describes various parameters of the VM for use by debuggers. See
    # tools/gen-postmortem-metadata.py for details.
    'v8_postmortem_support%': 'false',

    # Interpreted regexp engine exists as platform-independent alternative
    # based where the regular expression is compiled to a bytecode.
    'v8_interpreted_regexp%': 0,

    # Enable ECMAScript Internationalization API. Enabling this feature will
    # add a dependency on the ICU library.
    'v8_enable_i18n_support%': 0,

    # SRUK performance patches
    'sruk_fast_smi_elements': 'true',
    'sruk_string_replace_multiple': 'true',
    'sruk_native_quick_sort': 'true',
    'sruk_enable_osr_check': 'true',
    'sruk_latency_optimisations': 'true',
    'sruk_emit_copy_bytes': 'true',
    'sruk_trace_threshold_tuning': 'false',
    'sruk_and_ffff': 'true',
    'sruk_string_plus_object': 'true',
    'sruk_bit_rotate': 'false',
    'sruk_branch_optimisations': 'false',
    'sruk_for_in_loop': 'true',
    'sruk_number_untag_d': 'true',
    'sruk_parseint_div': 'true',
    'sruk_ispras_func_size': 'false',
    'sruk_ispras_dyn_func_check': 'false',
    'sruk_delete_movebytes': 'false',
    'sruk_page_scavenge_flag_break': 'false',
    'sruk_copywords': 'true',
    'sruk_clamp_to_8': 'true',
    'sruk_dead_code_elimination': 'false',
    'sruk_no_debugger_support': 'true', # Enable at release!
    'sruk_integer_mult_23452022': 'true',
    'sruk_load_store_multiple_44313002': 'true',
    'sruk_cache_stringindexof': 'true',
    'sruk_cache_stringtoarray': 'true',
    'sruk_cache_regexptest': 'true', # Prototype!
    'sec_ssrm_mode': 'true',
  },
  'target_defaults': {
    'conditions': [
      ['v8_enable_debugger_support==1', {
        'defines': ['ENABLE_DEBUGGER_SUPPORT',],
      }],
      ['v8_enable_disassembler==1', {
        'defines': ['ENABLE_DISASSEMBLER',],
      }],
      ['v8_enable_gdbjit==1', {
        'defines': ['ENABLE_GDB_JIT_INTERFACE',],
      }],
      ['v8_object_print==1', {
        'defines': ['OBJECT_PRINT',],
      }],
      ['v8_enable_verify_heap==1', {
        'defines': ['VERIFY_HEAP',],
      }],
      ['v8_interpreted_regexp==1', {
        'defines': ['V8_INTERPRETED_REGEXP',],
      }],
      ['v8_enable_i18n_support==1', {
        'defines': ['V8_I18N_SUPPORT',],
      }],
      ['v8_compress_startup_data=="bz2"', {
        'defines': [
          'COMPRESS_STARTUP_DATA_BZ2',
        ],
      }],
      [ 'sruk_fast_smi_elements=="true"', {
        'defines': [
          'SRUK_FAST_SMI_ELEMENTS',
        ],
      }],
      [ 'sruk_string_replace_multiple=="true"', {
        'defines': [
          'SRUK_STRING_REPLACE_MULTIPLE',
        ],
      }],
      [ 'sruk_native_quick_sort=="true"', {
        'defines': [
          'SRUK_NATIVE_QUICK_SORT',
        ],
      }],
      [ 'sruk_enable_osr_check=="true"', {
        'defines': [
          'SRUK_ENABLE_OSR_CHECK',
        ],
      }],
      [ 'sruk_latency_optimisations=="true"', {
        'defines': [
          'SRUK_LATENCY_OPTIMISATIONS',
        ],
      }],
      [ 'sruk_emit_copy_bytes=="true"', {
        'defines': [
          'SRUK_EMIT_COPY_BYTES',
        ],
      }],
      [ 'sruk_trace_threshold_tuning=="true"', {
        'defines': [
          'SRUK_TRACE_THRESHOLD_TUNING',
        ],
      }],
      [ 'sruk_and_ffff=="true"', {
        'defines': [
          'SRUK_AND_FFFF',
        ],
      }],
      [ 'sruk_string_plus_object=="true"', {
        'defines': [
          'SRUK_STRING_PLUS_OBJECT',
        ],
      }],
      [ 'sruk_bit_rotate=="true"', {
        'defines': [
          'SRUK_BIT_ROTATE',
        ],
      }],
      [ 'sruk_branch_optimisations=="true"', {
        'defines': [
          'SRUK_BRANCH_OPTIMISATIONS',
        ],
      }],
      [ 'sruk_for_in_loop=="true"', {
        'defines': [
          'SRUK_FOR_IN_LOOP',
        ],
      }],
      [ 'sruk_number_untag_d=="true"', {
        'defines': [
          'SRUK_NUMBER_UNTAG_D',
        ],
      }],
      [ 'sruk_parseint_div=="true"', {
        'defines': [
          'SRUK_PARSEINT_DIV',
        ],
      }],
      [ 'sruk_ispras_func_size=="true"', {
        'defines': [
          'SRUK_ISPRAS_FUNC_SIZE',
        ],
      }],
      [ 'sruk_ispras_dyn_func_check=="true"', {
        'defines': [
          'SRUK_ISPRAS_DYN_FUNC_CHECK',
        ],
      }],
      [ 'sruk_delete_movebytes=="true"', {
        'defines': [
          'SRUK_DELETE_MOVEBYTES',
        ],
      }],
      [ 'sruk_page_scavenge_flag_break=="true"', {
        'defines': [
          'SRUK_PAGE_SCAVENGE_FLAG_BREAK',
        ],
      }],
      [ 'sruk_copywords=="true"', {
        'defines': [
          'SRUK_COPYWORDS',
        ],
      }],
      [ 'sruk_clamp_to_8=="true"', {
        'defines': [
          'SRUK_CLAMP_TO_8',
        ],
      }],
      [ 'sruk_dead_code_elimination=="true"', {
        'defines': [
          'SRUK_DEAD_CODE_ELIMINATION',
        ],
      }],
      [ 'sruk_no_debugger_support=="true"', {
        'defines': [ 'SRUK_NO_DEBUGGER_SUPPORT'],
        'defines!': ['ENABLE_DEBUGGER_SUPPORT'],
      }],
      [ 'sruk_integer_mult_23452022=="true"', {
        'defines': [
          'SRUK_INTEGER_MULT_23452022',
        ],
      }],
      [ 'sruk_load_store_multiple_44313002=="true"', {
        'defines': [
          'SRUK_LOAD_STORE_MULTIPLE_44313002',
        ],
      }],
      [ 'sruk_cache_stringindexof=="true"', {
        'defines': [
          'SRUK_CACHE_STRINGINDEXOF',
        ],
      }],
      [ 'sruk_cache_stringtoarray=="true"', {
        'defines': [
          'SRUK_CACHE_STRINGTOARRAY',
        ],
      }],
      [ 'sruk_cache_regexptest=="true"', {
          'defines': [
            'SRUK_CACHE_REGEXPTEST',
          ],
      }],
      ['sec_ssrm_mode=="true"', {
        'defines': [
          'SEC_SSRM_MODE'
        ],
      }],
    ],  # conditions
    'configurations': {
      'Debug': {
        'variables': {
          'v8_enable_extra_checks%': 1,
        },
        'conditions': [
          ['v8_enable_extra_checks==1', {
            'defines': ['ENABLE_EXTRA_CHECKS',],
          }],
        ],
      },  # Debug
      'Release': {
        'variables': {
          'v8_enable_extra_checks%': 0,
        },
        'conditions': [
          ['v8_enable_extra_checks==1', {
            'defines': ['ENABLE_EXTRA_CHECKS',],
          }],
        ],  # conditions
      },  # Release
    },  # configurations
  },  # target_defaults
}
