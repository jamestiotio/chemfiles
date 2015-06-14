/*
 * Chemharp, an efficient IO library for chemistry file formats
 * Copyright (C) 2015 Guillaume Fraux
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/
*/

#ifndef HARP_FORMAT_HPP
#define HARP_FORMAT_HPP

#include <string>
#include <memory>
using std::shared_ptr;

#include "chemharp/File.hpp"

namespace harp {

class Frame;

/*!
 * @class Format Format.hpp Format.cpp
 * @brief Abstract format class
 *
 * Abstract base class for file formats reader and writer
 */
class Format {
public:
    Format() = default;
    virtual ~Format() = default;
    /*!
    * @brief Read a specific step from the internal file.
    * @param file The file to read from.
    * @param step The step to read
    * @param frame The frame to fill
    *
    * This function can throw an exception in case of error.
    */
    virtual void read_step(File* file, const size_t step, Frame& frame);

    /*!
    * @brief Read a specific step from a file.
    * @param file The file to read from.
    * @param frame The frame to fill
    *
    * This function can throw an exception in case of error. The cursor is
    * assumed to be at the right position in case of text files.
    */
    virtual void read(File* file, Frame& frame);

    /*!
    * @brief Write a step (frame) to a file.
    * @param file The file to read from.
    * @param frame The frame to be writen
    *
    * This function can throw an exception in case of error.
    */
    virtual void write(File* file, const Frame& frame);

    /*!
    * @brief Get the number of frames in a file
    * @param file The file to read from.
    * @return The number of frames
    */
    virtual size_t nsteps(File* file) const = 0;

    //! A short string describing the format.
    virtual std::string description() const = 0;
};

} // namespace harp

#endif