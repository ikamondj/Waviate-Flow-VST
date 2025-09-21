/*
  ==============================================================================

    StringifyDefines.h
    Created: 20 Sep 2025 7:53:46pm
    Author:  ikamo

  ==============================================================================
*/

#pragma once
#define NODE_CODE(code) code
#define NODE_CODE_STR(code) #code
#define DEFINE_AND_CREATE_VAR(code, codeVarName) NODE_CODE(code); const char* codeVarName = NODE_CODE_STR(code);