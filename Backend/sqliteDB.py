# sqlite_db.py
import os
import sqlite3
from contextlib import contextmanager
from datetime import datetime
from typing import Any, Iterable, Iterator, List, Optional

from flask import g, current_app

import logging
logger = logging.getLogger(__name__)


class SqliteDB:
    """
    Lightweight SQLite manager for Flask apps.

    Usage:
      db = SqliteDB("instance/app.db")
      db.init_app(app)

      # in a request:
      db.execute("INSERT INTO users (name) VALUES (?)", ("alice",))
      rows = db.fetchall("SELECT * FROM users WHERE id = ?", (1,))
    """

    def __init__(self, database: str, timeout: float = 5.0, detect_types: int = sqlite3.PARSE_DECLTYPES):
        self.database = database
        self.timeout = timeout
        self.detect_types = detect_types

    def init_app(self, app):
        """Attach this manager to a Flask app instance and register teardown."""
        # allow relative path from app.instance_path
        if not os.path.isabs(self.database):
            self.database = os.path.join(app.instance_path, self.database)

        # ensure instance folder exists
        os.makedirs(os.path.dirname(self.database), exist_ok=True)

        # store reference on app for easy retrieval
        if not hasattr(app, "extensions"):
            app.extensions = {}
        app.extensions.setdefault("sqlite_db", self)

        app.teardown_appcontext(self.teardown)

    def get_conn(self) -> sqlite3.Connection:
        """Return a per-request sqlite3.Connection stored on flask.g."""
        conn = getattr(g, "_sqlite_db_conn", None)
        if conn is None:
            conn = sqlite3.connect(self.database,
                                   timeout=self.timeout,
                                   detect_types=self.detect_types,
                                   check_same_thread=False)
            conn.row_factory = sqlite3.Row
            # sensible pragmas
            conn.execute("PRAGMA foreign_keys = ON")
            conn.execute("PRAGMA journal_mode = WAL")
            g._sqlite_db_conn = conn
        return conn

    def teardown(self, exc: Optional[BaseException] = None):
        """Close connection on appcontext teardown."""
        conn = getattr(g, "_sqlite_db_conn", None)
        if conn is not None:
            try:
                conn.close()
            except Exception:
                pass
            finally:
                g._sqlite_db_conn = None

    # Basic executors
    def execute(self, sql: str, params: Optional[Iterable[Any]] = None) -> sqlite3.Cursor:
        try:
            logger.info(f"Executing SQL: {sql} | Params: {params}")
            cur = self.get_conn().execute(sql, tuple(params) if params else ())
            self.get_conn().commit()
            logger.info("SQL executed successfully.")
            return cur
        except Exception as e:
            logger.error(f"SQL execution failed: {e}")
            raise

    def executemany(self, sql: str, seq_of_params: Iterable[Iterable[Any]]) -> sqlite3.Cursor:
        try:
            logger.info(f"Executing Many SQL: {sql}")
            cur = self.get_conn().executemany(sql, seq_of_params)
            self.get_conn().commit()
            logger.info("SQL executemany successful.")
            return cur
        except Exception as e:
            logger.error(f"SQL executemany failed: {e}")
            raise

    def executescript(self, sql_script: str) -> None:
        try:
            logger.info("Executing SQL script.")
            self.get_conn().executescript(sql_script)
            logger.info("SQL script executed successfully.")
        except Exception as e:
            logger.error(f"SQL script execution failed: {e}")
            raise

    # Fetch helpers that return dicts
    def fetchone(self, sql: str, params: Optional[Iterable[Any]] = None) -> Optional[dict]:
        cur = self.execute(sql, params)
        row = cur.fetchone()
        return dict(row) if row is not None else None

    def fetchall(self, sql: str, params: Optional[Iterable[Any]] = None) -> List[dict]:
        cur = self.execute(sql, params)
        return [dict(row) for row in cur.fetchall()]

    # Transaction context manager
    @contextmanager
    def transaction(self) -> Iterator[sqlite3.Connection]:
        conn = self.get_conn()
        try:
            yield conn
            conn.commit()
        except Exception:
            conn.rollback()
            raise

    # Simple schema initialization
    def init_db(self, schema_path: str) -> None:
        """Run a schema SQL file (absolute path or relative to instance folder)."""
        if not os.path.isabs(schema_path):
            schema_path = os.path.join(os.path.dirname(self.database), schema_path)
        with open(schema_path, "r", encoding="utf8") as f:
            sql = f.read()
        self.executescript(sql)

    # Simple migration runner
    def migrate(self, migrations_dir: str) -> List[str]:
        """
        Apply pending SQL migration files from `migrations_dir`.
        Files must be named with a sortable prefix, e.g. 001_init.sql, 002_add_users.sql.

        Returns list of applied filenames.
        """
        # create migrations table if missing
        self.executescript(
            "CREATE TABLE IF NOT EXISTS schema_migrations (version TEXT PRIMARY KEY, applied_at TEXT NOT NULL);"
        )

        # resolve dir path relative to app instance if needed
        if not os.path.isabs(migrations_dir):
            migrations_dir = os.path.join(os.path.dirname(self.database), migrations_dir)
        if not os.path.isdir(migrations_dir):
            return []

        files = sorted(f for f in os.listdir(migrations_dir) if f.lower().endswith(".sql"))
        applied = []
        for fname in files:
            version = fname  # could use numeric prefix if you prefer
            cur = self.execute("SELECT 1 FROM schema_migrations WHERE version = ?", (version,))
            if cur.fetchone() is not None:
                continue  # already applied

            path = os.path.join(migrations_dir, fname)
            with open(path, "r", encoding="utf8") as f:
                sql = f.read()

            # apply inside a transaction
            with self.transaction():
                self.executescript(sql)
                self.execute(
                    "INSERT INTO schema_migrations (version, applied_at) VALUES (?, ?)",
                    (version, datetime.utcnow().isoformat())
                )
            applied.append(fname)
        return applied
