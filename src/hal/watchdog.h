#pragma once
// Guard active work only. Deliberate indefinite WFI is unarmed; no idle tick.
void watchdog_arm(void);
void watchdog_disarm(void);
void watchdog_feed(void);
void watchdogs_disable(void);
