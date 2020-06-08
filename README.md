# Service providing automatic spell checking.

Based on [Burkhard-Keller tree](https://dl.acm.org/doi/10.1145/362003.362025). 

### Quickstart

1. Install dependencies: 
   ```bash
   apt install libpoco-dev cmake g++-8
   ```
2. Build:
    ```bash
   mkdir build && cd build && cmake .. && make
    ```
3. Run on ```0.0.0.0:9000``` 
    ```bash
    ./web_app --dictionary_path ../databases/name_surname.txt --address 0.0.0.0 --port 9000
    ```
4. Use (for example from python)
   ```python
    import requests
    requests.post("http://localhost:9000/correct", 
       json=[{'candidate' : 'Александр', 'max_tolerance': 1}]).text
    ```
   You may create list of requests as a batch and receive list of responses
   Response:
   ```json
   [
       {
           "milliseconds": 82,
           "results": [
               {
                   "priority": 10000000,
                   "tolerance": 0,
                   "word": "Александр"
               },
               {
                   "priority": 121000,
                   "tolerance": 1,
                   "word": "Олександр"
               },   
     ...
    ```
    
### Creating your own dictionary
Dictionary is text file. Each line consists from string ```word``` and unsingned int ```priority```. 
The higher the priority of a word, the higher it will appear in the result list. 
For example, priority is frequence of word in some database. If one word is mentioned twice, priorities will be added.

### Custom metric
By default, the case-sensitive Levenshtein metric is used. But you can create your custom weighted metric, and pass config file via flag ```--metric_config=../metric_config.json```
```metric_config.json```
```json
{
  "default": {
    "insert_delete": 2,
    "replace": 1,
    "case_sensitive": true
  },
  "custom_insert_delete": [
    {
      "group": "1",
      "cost": 5,
      "_comment": "insertion and deletion of \"1\" costs 5"
    },
    {
      "group": "!?#$",
      "cost": 4,
      "_comment": "each this insertions or deletions costs 4"
    }
  ],
  "custom_replace": [
    {
      "first_group": "5",
      "second_group": "6",
      "cost": 4,
      "_comment": "replace 5 to 6 costs 4"
    },
    {
      "first_group": "56",
      "second_group": "b",
      "cost": 5,
      "_comment": "5->b, 6->b, b->5, b->6 costs 5"
    },
    {
      "first_group": "0d",
      "second_group": "c8",
      "cost": 7,
      "_comment": "0->c 0->8 d->c d->8, c->0, c->d, 8->0, 8->d costs 7"
    }
  ]
}
```
