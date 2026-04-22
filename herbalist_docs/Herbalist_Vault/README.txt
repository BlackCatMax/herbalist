# Полная обработка (проверка + исправление + бэкап + отчёт)
python mega_docs.py --all

# Только проверка (без изменений)
python mega_docs.py --check

# Только исправление
python mega_docs.py --fix

# Исправление с бэкапом
python mega_docs.py --fix --backup

# Показать существующий отчёт
python mega_docs.py --report