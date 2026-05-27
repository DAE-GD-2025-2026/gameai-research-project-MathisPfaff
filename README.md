# Utility Theory for AI Decision-Making

## Overview

Utility theory in AI decision-making is a way to choose between possible actions by using **utility scores**. A utility score represents how useful or needed an action is in the given situation.

Rather than using if/else logic, a utility-based system compares multiple possible choices by scoring them using relevant factors and selects the option with the highest score.

This system is especially useful for agents in any environment where decisions must adapt to changing conditions.

---

## Core Idea

A utility-based AI system follows these steps:

1. Identifying possible actions
2. Gathering relevant context
3. Evaluating each action using considerations
4. Converting gathered data into normalized scores [0, 1]
5. Combining those scores into a final utility value
6. Choosing the highest-utility option

In simple terms:

**context → considerations → utility scores → decision**

---

