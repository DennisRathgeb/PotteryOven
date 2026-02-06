# PotteryOven Code Improvements Tracker

This document tracks identified code quality issues, potential bugs, and areas for improvement in the PotteryOven kiln controller project.

**Total Issues Identified: 53**

---

## 1. Naming Inconsistencies (5 issues)

| ID | File | Line | Issue | Severity |
|----|------|------|-------|----------|
| N1 | encoder.c | 14 | `init_Encoder` uses mixed case while other modules use `initHeater`, `initUI`, `initLog` pattern | Low |
| N2 | MAX31855.c | 152,189 | Function names `max_31855_print_payload` and `max_31855_print_max31855_payload_binary` use underscore after `max` inconsistently with `max31855_*` pattern | Low |
| N3 | pid.h | 36 | `PID_HandletypeDef_t` has lowercase 't' in 'type' while other typedefs use `HandleTypeDef_t` | Low |
| N4 | Various | - | Author field inconsistent: "burni" vs "Dennis Rathgeb" in file headers | Low |
| N5 | heater.h | 87 | Comment typo "temparuter" should be "temperature" | Low |

---

## 2. Error Handling Issues (4 issues)

| ID | File | Line | Issue | Severity |
|----|------|------|-------|----------|
| E1 | event.c | 65-66 | `exit(EXIT_FAILURE)` called on malloc failure - inappropriate for embedded systems, will cause hard fault | Critical |
| E2 | event.c | 87-88 | `exit(EXIT_FAILURE)` called on empty queue dequeue - should return error status instead | Critical |
| E3 | event.c | 64 | `fprintf(stderr, ...)` used - stderr may not be configured on embedded target | Medium |
| E4 | MAX31855.c | 68-73 | HAL_SPI_Receive return value checked but error not propagated to caller with context | Low |

---

## 3. Memory Management Issues (3 issues)

| ID | File | Line | Issue | Severity |
|----|------|------|-------|----------|
| M1 | event.c | 62 | `malloc()` used for event nodes - dynamic allocation in ISR context can cause fragmentation and timing issues | High |
| M2 | event.c | - | No memory pool or static allocation alternative for event queue nodes | High |
| M3 | event.c | 93 | `free()` called in `event_dequeue` - pairs with malloc but still problematic for real-time embedded | Medium |

---

## 4. Critical TODOs / Unimplemented Features (7 issues)

| ID | File | Line | Issue | Severity |
|----|------|------|-------|----------|
| T1 | heater.c | 364-367 | PID calculation is TODO - slope and mean calculated but not used for control | Critical |
| T2 | MAX31855.c | 66 | TODO: Non-blocking SPI implementation (DMA) not done | Medium |
| T3 | heater.c | 112 | TODO: RTC-based timing to avoid uint32_t overflow in PWM timing | Medium |
| T4 | MAX31855.h | 21 | TODO: Test if bit order is correct for bitfield struct | High |
| T5 | MAX31855.h | 39 | TODO: Investigate union implementation (commented code present) | Low |
| T6 | ui.c | 318 | TODO: Start functionality not implemented (BUT4 handler) | Medium |
| T7 | ui.c | 717 | TODO: Setpoint choose functionality not implemented | Medium |

---

## 5. Magic Numbers (8 issues)

| ID | File | Line | Issue | Severity |
|----|------|------|-------|----------|
| MN1 | heater.c | 21,151,174,180 | `0xff` used as sentinel value for `heater_level_prev` - should be defined constant | Medium |
| MN2 | encoder.c | 38 | `0xff` used as magic value for pin comparison - unclear meaning | Medium |
| MN3 | MAX31855.c | 38-47 | Bit shift values (31, 20, 18, etc.) should be named constants | Low |
| MN4 | lcd1602_rgb.c | 79,128,134 | `0x80`, `0x40`, `0xc0` magic values for I2C commands | Low |
| MN5 | lcd1602_rgb.c | 157-158,169-170 | `0x07`, `0x17`, `0x06`, `0x7f`, `0xff` for LED blink registers | Low |
| MN6 | lcd1602_rgb.c | 241 | `0x7` mask for custom symbol location | Low |
| MN7 | ui.c | 40-59 | Custom character bitmaps defined inline - could be constants | Low |
| MN8 | log.c | 44 | Buffer size 256 is hardcoded - should be configurable define | Low |

---

## 6. Potential Bugs (6 issues)

| ID | File | Line | Issue | Severity |
|----|------|------|-------|----------|
| B1 | encoder.h | 37 | `position` is `int` type but getter returns `uint8_t*` - type mismatch, potential truncation | High |
| B2 | MAX31855.c | 62 | NULL check `&hmax31855->hspi` checks address of pointer, not pointer itself - should be `hmax31855->hspi` | High |
| B3 | heater.c | 358 | Array index `hheater->time_counter - 1` could be -1 if counter is 0 (though condition should prevent) | Medium |
| B4 | ui.c | 493,523 | `cur_index` is `uint8_t` - decrementing from 0 causes wrap to 255, then compared `>= length` | Medium |
| B5 | pid.c | 16-18 | Static variables `integral`, `last_error`, `last_derivative` are global - prevents multiple PID instances | Medium |
| B6 | ui.c | 441,445,460,464 | `scroll_counter` bounds checking uses `length` (programs.length) instead of `program.length` | High |

---

## 7. Missing Bounds Checks (5 issues)

| ID | File | Line | Issue | Severity |
|----|------|------|-------|----------|
| BC1 | heater.c | 358 | `temperature[]` array access without explicit bounds check | Medium |
| BC2 | ui.c | 326 | `program_list[index]` access - `index` could exceed `MAX_PROGRAMS` | Medium |
| BC3 | ui.c | 179,189-190 | `temperature[]`, `gradient[]`, `gradient_negative[]` access with `(scroll_counter-1)/2` | Medium |
| BC4 | heater.c | 203-248 | `heater_set_level` accepts any uint8_t but only handles 0-6 | Low |
| BC5 | lcd1602_rgb.c | 241 | `location &= 0x7` silently masks invalid locations instead of returning error | Low |

---

## 8. Inconsistent Return Value Handling (4 issues)

| ID | File | Line | Issue | Severity |
|----|------|------|-------|----------|
| R1 | heater.c | 263-265 | `heater_set_coil_state` return values ignored for coils | Medium |
| R2 | ui.c | 89-90 | `lcd1602_customSymbol` return values ignored | Low |
| R3 | lcd1602_rgb.c | 33,41,48 | `lcd1602_command` return values summed into `err_c` - HAL_OK is 0, HAL_ERROR is 1, but sum loses specific error | Low |
| R4 | Various | - | Many functions return HAL_StatusTypeDef but callers don't check | Medium |

---

## 9. Dead Code / Debug Code (4 issues)

| ID | File | Line | Issue | Severity |
|----|------|------|-------|----------|
| D1 | MAX31855.c | 138-240 | Large block of debug print functions marked TODO:REMOVE | Low |
| D2 | MAX31855.h | 40-57 | Commented-out union implementation | Low |
| D3 | ui.c | 199-242 | Commented-out template function `ui_update_` | Low |
| D4 | encoder.c | 57,74,81 | Commented-out printf debug statements | Low |

---

## 10. Documentation Gaps (7 issues)

| ID | File | Line | Issue | Severity |
|----|------|------|-------|----------|
| DOC1 | heater.c | 272-311 | `heater_calculate_slope` algorithm (linear regression) not documented | Medium |
| DOC2 | ui.c | - | State machine transitions not documented | Medium |
| DOC3 | lcd1602_rgb.c | - | No file header comment, no function documentation | Medium |
| DOC4 | pid.c | 41-63 | PID algorithm with derivative filtering not explained | Medium |
| DOC5 | MAX31855.h | 22-37 | Bitfield struct format not fully explained (MSB/LSB order) | Medium |
| DOC6 | encoder.h | 15-21 | Comment exists but interrupt mechanism not fully explained | Low |
| DOC7 | heater.h | 31-43 | Usage comment exists but door safety behavior not fully documented | Low |

---

## Priority Summary

| Severity | Count |
|----------|-------|
| Critical | 4 |
| High | 6 |
| Medium | 26 |
| Low | 17 |

### Recommended Fix Order

1. **Critical (fix immediately)**
   - E1, E2: Replace `exit()` calls with proper error handling
   - T1: Implement PID control loop
   - T4: Verify MAX31855 bitfield byte order

2. **High (fix soon)**
   - M1, M2: Replace malloc with static memory pool for events
   - B1: Fix encoder position type mismatch
   - B2: Fix NULL check in MAX31855
   - B6: Fix scroll_counter bounds checking in ui.c

3. **Medium (scheduled fixes)**
   - Remaining items

4. **Low (opportunistic)**
   - Naming consistency, comments, dead code removal

---

## Changelog

| Date | Author | Changes |
|------|--------|---------|
| 2024-XX-XX | Claude | Initial analysis and documentation |
