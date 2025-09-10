/*
  ==============================================================================

    InputType.h
    Created: 9 Sep 2025 7:14:17pm
    Author:  ikamo

  ==============================================================================
*/

#pragma once
enum class InputType {
    decimal = 0,
    boolean = 1,
    integer = 2,
    any = 3,
    followsInput = 4,
    dirty = -1,
};