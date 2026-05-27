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

---

## How Utility Scoring Works

Each action gets a score through one or more considerations.

For example, a **Heal** action might depend on:

- current health
- distance to safety
- nearby enemies

Each of those inputs is turned into a score, usually in the range **0.0 to 1.0**, where:

- `0.0` = not useful
- `1.0` = highly useful

These scores are then combined into a final utility score for the action.

The AI compares all available actions and selects the one with the highest score.

---

## Why Normalize Scores

Utility systems often normalize values to a range of **0 to 1**.

Benefits of normalization:
- easier comparison between different factors
- easier combination with multiple considerations
- more predictable tuning
- cleaner balancing across actions

Example:
- health could be converted from `4/10` to `0.4`
- distance could be scaled into a similar range
- amount of nearby allies can be inverted, the more allies the lower the score

---

## Scoring Curves

Raw inputs are often not used directly. Instead, they are passed through **scoring curves** that shape how strongly a factor influences the decision.

This matters because decision pressure is often not linear.

Example:
- losing a little health may not matter much
- but very low health will increase the need to heal by a lot

### Common Curve Types

- **Linear**: straight-line scaling
<img width="320" height="320" alt="linear" src="https://github.com/user-attachments/assets/89b74d71-0319-46b6-a1b9-7755c1bd2afb" />

- **Exponential**: grows rapidly
<img width="320" height="320" alt="exponential" src="https://github.com/user-attachments/assets/5e7e523f-8b44-4800-a75e-9967039feffc" />

- **Sine**: smooth easing behavior
<img width="320" height="320" alt="sine" src="https://github.com/user-attachments/assets/13623b23-99c3-4ee1-b31d-c3f4c5bbfe6b" />

- **Cosine**: alternative smooth shaping
<img width="320" height="320" alt="cosine" src="https://github.com/user-attachments/assets/d41aee60-261d-4bd5-a72d-747ef1e82504" />

- **Logistic**: S-shaped growth
<img width="320" height="320" alt="logistic" src="https://github.com/user-attachments/assets/5e0a8061-9ece-4e41-a801-cbc5a6abc9a0" />

- **Logit**: inverse-style shaping
<img width="320" height="320" alt="logit" src="https://github.com/user-attachments/assets/637720ec-6481-4b0d-a4b0-205ab32d17bd" />

- **Smoothstep**: smooth transition
<img width="320" height="320" alt="smoothstep" src="https://github.com/user-attachments/assets/320975b4-586d-4ea7-b57f-e1d0b5a4918e" />

- **Smootherstep**: even smoother transition
<img width="320" height="320" alt="smootherstep" src="https://github.com/user-attachments/assets/e281d98d-0b2d-47a6-aab0-405d3275a2e2" />


Choosing the right curve is an important part of designing a utility system.
You can make your own curves to fit the situation, the output of the curve just needs to be in the range [0, 1].

---

## Example Decision Flow

Imagine an AI-controlled character with these possible actions:

- Attack
- Retreat
- Heal

### Context
- health = 0.20
- enemy distance = 0.30
- ammo = 0.90
- danger = 0.80

### Example Considerations

#### Attack
- enemy is close
- ammo is high
- health is not critically low

#### Retreat
- danger is high
- health is low

#### Heal
- health is very low
- current position is somewhat safe

Each action gets a utility score based on these factors, and the AI picks the highest-scoring option.

---

## Two Useful Ways To Use Utility AI

### 1. Choose the Best Action
This is the standard utility AI pattern.

One actor evaluates several actions and picks the one with the highest utility.

### 2. Choose the Best Actor for an Action
Utility scoring can also be used to decide **who** should perform a task.

Example:
- Which medic should heal an injured ally?
- Which worker should grab that resource?
- Which unit is best to stay and defend?

In this pattern, the same scoring logic is applied across multiple actors to find the best candidate.
<img width="547" height="275" alt="elements4" src="https://github.com/user-attachments/assets/e55ef926-6979-46f3-b6d0-2d33fdeae659" />


---

## Decision vs Execution

A useful design principle is to separate:

- **decision-making**
- **action execution**

The system first decides what should happen, then executes the chosen action afterward.

Benefits:
- cleaner architecture
- easier debugging
- better reuse of decision logic

---

## Why Utility Theory Is Useful

Utility theory helps AI behave more flexibly than rule-only systems.

Advantages:
- supports subtle decisions
- good for changing environments
- allows smooth balancing and tuning
- scales better as behaviors grow more complex

Instead of saying:

"If health < 30, always flee"

a utility system can express:

- healing becomes more desirable as health drops
- fleeing becomes more desirable as enemies come close
- attacking remains possible if other conditions are favorable

This creates more adaptable behavior.

---

## Design Principles

A strong utility AI system depends on good modeling.

Important design choices include:
- what actions exist
- what context is collected
- what considerations are used
- how each consideration is scored
- what curve shapes are applied
- how scores are combined

---

## Basic Implementation

*[to be added]*

---

## Summary

Utility theory for AI decision-making is a structured way to choose the best option based on context.

It works by:
- collecting situational data
- evaluating actions through considerations
- transforming inputs with scoring curves
- combining scores into utility values
- choosing the highest-scoring result

---

## Credits

Source and Images: [AI Decision Making with Utility Scores, Part 1](https://mcguirev10.com/2019/01/03/ai-decision-making-with-utility-scores-part-1.html)

Source: [GameAIPro_Chapter09_An_Introduction_to_Utility_Theory](https://www.gameaipro.com/GameAIPro/GameAIPro_Chapter09_An_Introduction_to_Utility_Theory.pdf?utm_source=openai)
