/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/display/SAT-display.hpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It contains the display for the SAT solver.
 */
#pragma once

#include "SAT-config.hpp"
#include "SAT-types.hpp"
#include "../observer/SAT-observer.hpp"

#include <string>

namespace napsat::gui
{
  class observer;

  class display
  {
  private:
    /**
     * @brief The observer that is notified of the SAT solving progress and keeps track of the state of the solver.
     */
    observer* _observer;

    /**
     * @brief The granularity level of the display. It decided how often the display is refreshed and asks for user input.
     */
    unsigned _display_level = 1;

    /**
     * @brief Whether the display has been updated since the last user input.
     * If it has not, the display is not refreshed.
    */
    bool _updated = true;

    /**
     * @brief Set the display level.
     */
    void set_display_level(unsigned level);

    /**
     * @brief Return back on step in the past, to the last event whose level is lower than the current display level.
     */
    void back();

    /**
     * @brief Goes to the next event whose level is lower than the current display level.
     * @return true if the display is back to real time.
     */
    bool next();

  public:
    /**
     * @brief Construct a new display object
     * @param observer The observer that contains the solver state.
     */
    display(observer* observer) : _observer(observer) {}

    /**
     * @brief Notify the display that an event of level <level> has ocurred.
     * If the level is lower than the display level, the display is updated.
     * @param level The level of the event.
     */
    void notify_change(unsigned level);

    /**
     * @brief Notify that a checkpoint has been reached, waiting for a user input.
     */
    void notify_checkpoint();

    /**
     * @brief Print the current state of the solver. This is a primitive display for POC purposes.
     */
    void print_state();
  };
}
