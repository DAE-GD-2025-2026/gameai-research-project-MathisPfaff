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

## Main Components

### Actor
The entity making the decision.

### Actions
The possible things the actor can do.

Examples:
- attack
- flee
- heal
- reload
- gather resources
- defend

### Context
The data that describes the current situation.

Examples:
- current health
- distance to target
- ammo count
- status of other agents

### Considerations
The factors used to evaluate that an action is a good choice or not.

Examples:
- "How low is my health?"
- "How close is the enemy?"
- "How low is my ammo?"

Each consideration reads from context and makes a score.
<img width="740" height="276" alt="elements2" src="https://github.com/user-attachments/assets/20075fbb-2bc6-4118-8835-0ffe831661a4" />

<img width="738" height="278" alt="elements3" src="https://github.com/user-attachments/assets/0c54f25c-e81f-46c1-a794-05570d8e6849" />

Source: [AI Decision Making with Utility Scores, Part 1](https://mcguirev10.com/2019/01/03/ai-decision-making-with-utility-scores-part-1.html)


---


## Credits

[AI Decision Making with Utility Scores, Part 1](https://mcguirev10.com/2019/01/03/ai-decision-making-with-utility-scores-part-1.html)
