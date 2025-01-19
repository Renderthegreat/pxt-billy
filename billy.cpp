/*
 * MIT License
 *
 * Copyright (c) 2023 Adam Granger
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "billy.h"
#include "pxt.h"

namespace billy {

Billy::Billy() : sampleSource(nullptr), outputBufferFill(0), outputBufferCount(0), inputPosOffset(0) {
    memset(outputBuffer, 0, sizeof(outputBuffer));
}

void Billy::init() {
    uBit.audio.requestActivation();

    if (sampleSource == nullptr) {
        sampleSource = new MemorySource();
        sampleSource->setFormat(DATASTREAM_FORMAT_8BIT_UNSIGNED);
        sampleSource->setBufferSize(outputBufferSize);
        uBit.audio.mixer.addChannel(*sampleSource, sampleRate, 255);
    }
}

void Billy::flushRemainder() {
    if (outputBufferFill > 0 && sampleSource != nullptr) {
        sampleSource->play(outputBuffer, outputBufferFill);
        outputBufferFill = 0; // Reset the buffer fill after flushing
    }
}

void Billy::configureVoice(int speed, int pitch, int mouth, int throat) {
    this->speed = speed;
    this->pitch = pitch;
    this->mouth = mouth;
    this->throat = throat;
}

void Billy::say(const char *words) {
    if (words == nullptr) {
        uBit.display.print('X');
        return;
    }

    char input[256] = {0};
    size_t length = strlen(words);

    // Ensure length does not exceed safe limits
    if (length > 80) {
        length = 80;
    }

    strncpy(input, words, length);

    // Convert words to phonemes
    if (billy::TextToPhonemes(reinterpret_cast<unsigned char *>(input)) == 0) {
        uBit.display.print('X');
        return;
    }

    pronounce(input, false);
}

void Billy::pronounce(const char *phonemes, bool sing) {
    if (phonemes == nullptr) {
        uBit.display.print('X');
        return;
    }

    char input[256] = {32}; // Initialize with spaces
    size_t length = strlen(phonemes);

    // Ensure input length does not exceed buffer size
    if (length > 255) {
        length = 255;
    }

    strncpy(input, phonemes, length);
    input[length] = -101; // End marker

    // Configure voice parameters
    SetSingmode(sing ? 1 : 0);
    SetSpeed(speed);
    SetPitch(pitch);
    SetMouth(mouth);
    SetThroat(throat);

    resetBuffer();

    // Set input and process speech synthesis
    billy::SetInput(input);
    if (billy::SAMMain() == 0) {
        uBit.display.print('X');
    }

    flushRemainder();
}

void Billy::resetBuffer() {
    inputPosOffset = 0;
    outputBufferFill = 0;
    outputBufferCount = 0;
    memset(outputBuffer, 0, sizeof(outputBuffer));
}

void Billy::outputByte(unsigned int pos, unsigned char value) {
    if (sampleSource == nullptr) {
        return;
    }

    int offset = pos - inputPosOffset;

    if (offset >= 0 && offset < outputBufferSize) {
        outputBuffer[offset] = value;

        if (offset >= outputBufferFill) {
            outputBufferFill = offset + 1; // Update high-water mark
        }
    }

    unsigned int newOutputBufferCount = pos / outputBufferSize;
    if (newOutputBufferCount > outputBufferCount) {
        outputBufferCount = newOutputBufferCount;
        sampleSource->play(outputBuffer, outputBufferSize);
        outputBufferFill = 0;
        inputPosOffset = outputBufferCount * outputBufferSize;
    }
}

}
