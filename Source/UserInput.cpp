/*
  ==============================================================================

    UserInput.cpp
    Created: 11 Aug 2025 5:35:21pm
    Author:  ikamo

  ==============================================================================
*/

#include "UserInput.h"

double UserInput::getHistoricalSample(int samplesAgo) const
{
    return leftInputHistory[samplesAgo] * !isStereoRight + rightInputHistory[samplesAgo] * isStereoRight;
}
