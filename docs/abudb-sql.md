# AbuSQL + AbuDB Example Ecosystem

## Overview

This example demonstrates:

* `.datac` database files
* AbuDB structure
* AbuSQL querying
* IMPORT / CONTENT system
* ASC / DESC / ABC / ZYX ordering
* GROUP
* aggregates (`SUM`, `MEAN`, `MAX`, `MIN`, `SIZE`)
* filtering
* updating
* cropping
* relational dependency concepts
* AC compatibility

---

# Example `.datac` Files

## `students.datac`

```datac
class string property: name
int sub.name: age
int sub.name: grade
string sub.name: house

students {
    name: "Alex", age: 16, grade: 91, house: "Red"
    name: "Sam", age: 17, grade: 88, house: "Blue"
    name: "Jordan", age: 15, grade: 97, house: "Red"
    name: "Taylor", age: 16, grade: 74, house: "Green"
    name: "Morgan", age: 17, grade: 82, house: "Blue"
}
```

---

## `inventory.datac`

```datac
class string property: item
int sub.item: stock
int sub.item: cost
string sub.item: category

inventory {
    item: "Keyboard", stock: 42, cost: 30, category: "Hardware"
    item: "Mouse", stock: 58, cost: 20, category: "Hardware"
    item: "Monitor", stock: 15, cost: 220, category: "Display"
    item: "Cable", stock: 120, cost: 8, category: "Accessory"
}
```

---

## `scores.datac`

```datac
class string property: player
int sub.player: score
int sub.player: level

scores {
    player: "Alex", score: 120, level: 5
    player: "Sam", score: 90, level: 4
    player: "Jordan", score: 150, level: 7
    player: "Taylor", score: 110, level: 5
}
```

---

# Importing Databases

## Import a `.datac` file

```abusql
IMPORT students.datac TO students
IMPORT inventory.datac TO inventory
IMPORT scores.datac TO scores
```

---

## Import spreadsheet data

```abusql
CONTENT FROM students.xlsx TO students
CONTENT FROM inventory.csv TO inventory
```

---

# Basic Querying

## Take everything

```abusql
TAKE * FROM students
```

---

## Take specific properties

```abusql
TAKE name, grade FROM students
```

---

## Filtering

```abusql
TAKE name, grade FROM students
IF grade >= 90
```

---

## Multiple conditions

```abusql
TAKE * FROM students
IF grade >= 80 & age >= 16
```

---

## OR logic

```abusql
TAKE * FROM students
IF house = "Red" | house = "Blue"
```

---

## NOT logict->least impact on AC's legacy, give each a point from 1->10, 1 being the right one, 10 being the left one

```abusql
TAKE * FROM students
IF !house = "Green"
```

---

# Ordering

## Numeric ascending

```abusql
TAKE * FROM students
ORDER BY grade ASC
```

---

## Numeric descending

```abusql
TAKE * FROM students
ORDER BY grade DESC
```

---

## Alphabetical

```abusql
TAKE * FROM students
ORDER BY name ABC
```

---

## Reverse alphabetical

```abusql
TAKE * FROM students
ORDER BY name ZYX
```

---

# Aggregates

## Sum

```abusql
TAKE SUM(grade) FROM students
```

---

## Mean

```abusql
TAKE MEAN(grade) FROM students
```

---

## Maximum

```abusql
TAKE MAX(grade) FROM students
```

---

## Minimum

```abusql
TAKE MIN(grade) FROM students
```

---

## Count rows

```abusql
TAKE SIZE(students)
```

---

# Grouping

## Group by house

```abusql
TAKE house, MEAN(grade)
FROM students
GROUP house
```

---

## Group inventory by category

```abusql
TAKE category, SUM(stock)
FROM inventory
GROUP category
```

---

# Updating Data

## Update values

```abusql
UPDATE students
SET grade = 95
IF name = "Sam"
```

---

## Increase values

```abusql
UPDATE inventory
SET stock += 10
IF item = "Monitor"
```

---

# Inserting Data

## Insert row

```abusql
INSERT students {
    name: "Casey",
    age: 16,
    grade: 84,
    house: "Yellow"
}
```

---

# Removing Data

## Remove rows

```abusql
RM FROM students
IF grade < 60
```

---

# Cropping

## Permanently keep top 3 grades

```abusql
CROP 3 DESC FROM students BY grade
```

---

## Keep first 10 alphabetically

```abusql
CROP 10 ABC FROM students BY name
```

---

## Keep bottom 5 grades

```abusql
CROP 5 ASC FROM students BY grade
```

---

# Limiting Views

## Top rows

```abusql
TAKE * FROM students
TOP 3
```

---

## Bottom rows

```abusql
TAKE * FROM students
BOTTOM 2
```

---

## Middle range

```abusql
TAKE * FROM students
MIDDLE 2 4
```

---

# Relational Dependency Example

## `classes.datac`

```datac
class string property: class
string sub.class: student

classes {
    class: "Math", student: "Alex"
    class: "Science", student: "Jordan"
    class: "History", student: "Sam"
}
```

---

## Connect datasets

```abusql
CONNECT students.name TO classes.student
```

---

## Joined query

```abusql
TAKE students.name, students.grade, classes.class
FROM students, classes
IF students.name = classes.student
```

---

# Full Example Script

```abusql
IMPORT students.datac TO students
IMPORT classes.datac TO classes

TAKE students.name, students.grade, classes.class
FROM students, classes
IF students.name = classes.student & students.grade >= 85
ORDER BY students.grade DESC
TOP 3
```

---

# Example Output

```text
Jordan | 97 | Science
Alex   | 91 | Math
Sam    | 88 | History
```

---

# AbuSQL Philosophy

AbuSQL is designed to:

* look readable like English
* avoid unnecessary symbols
* simplify database querying
* integrate directly with `.datac`
* work cleanly with AC ecosystem tooling
* separate storage logic from AC/AI runtime logic

It combines ideas from:

* SQL
* spreadsheets
* English-like scripting
* lightweight database engines
* structured data systems
