#include "storage/Database.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

#include <array>
#include <utility>

namespace sheaf {

namespace {

// Migration N (1-based) lives at index N-1. Never edit an released migration; append a new one.
constexpr std::array<const char*, 1> kMigrations = {
    // 1: initial schema
    R"sql(
CREATE TABLE documents (
  file_key   TEXT PRIMARY KEY,
  last_path  TEXT NOT NULL,
  page       INTEGER NOT NULL DEFAULT 0,
  offset_y   REAL    NOT NULL DEFAULT 0,
  zoom       REAL    NOT NULL DEFAULT 1,
  updated_at INTEGER NOT NULL
);
CREATE TABLE groups (
  id        INTEGER PRIMARY KEY,
  name      TEXT    NOT NULL,
  position  INTEGER NOT NULL,
  collapsed INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE tabs (
  id       INTEGER PRIMARY KEY,
  group_id INTEGER NOT NULL REFERENCES groups(id) ON DELETE CASCADE,
  path     TEXT    NOT NULL,
  file_key TEXT,
  position INTEGER NOT NULL
);
CREATE TABLE settings (key TEXT PRIMARY KEY, value TEXT);
)sql",
};

constexpr int kLatestVersion = static_cast<int>(kMigrations.size());

}  // namespace

int Database::LatestSchemaVersion() {
  return kLatestVersion;
}

QString Database::DefaultPath() {
  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/sheaf.db";
}

Database::Database(QString connection_name) : connection_name_(std::move(connection_name)) {
}

Database::~Database() {
  Close();
}

bool Database::Open(const QString& path) {
  Close();
  error_.clear();

  if (path != ":memory:") {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
      error_ = "Cannot create directory for " + path;
      return false;
    }
  }

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connection_name_);
  db.setDatabaseName(path);
  if (!db.open()) {
    error_ = db.lastError().text();
    Close();
    return false;
  }

  QSqlQuery pragma(db);
  if (!pragma.exec("PRAGMA foreign_keys = ON")) {
    error_ = pragma.lastError().text();
    Close();
    return false;
  }

  if (!Migrate()) {
    Close();
    return false;
  }
  return true;
}

void Database::Close() {
  if (QSqlDatabase::contains(connection_name_)) {
    {
      QSqlDatabase db = QSqlDatabase::database(connection_name_, false);
      if (db.isOpen()) {
        db.close();
      }
    }
    QSqlDatabase::removeDatabase(connection_name_);
  }
}

bool Database::IsOpen() const {
  return QSqlDatabase::contains(connection_name_) && Handle().isOpen();
}

QString Database::Error() const {
  return error_;
}

QSqlDatabase Database::Handle() const {
  return QSqlDatabase::database(connection_name_, false);
}

int Database::SchemaVersion() const {
  QSqlQuery query(Handle());
  if (!query.exec("SELECT version FROM schema_version") || !query.next()) {
    return 0;
  }
  return query.value(0).toInt();
}

bool Database::Migrate() {
  QSqlDatabase db = Handle();
  QSqlQuery query(db);

  // All migrations run in one transaction, so a crash never leaves a half-migrated database.
  if (!db.transaction()) {
    error_ = db.lastError().text();
    return false;
  }

  auto fail = [&](const QString& message) {
    error_ = message;
    db.rollback();
    return false;
  };

  if (!query.exec("CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL)")) {
    return fail(query.lastError().text());
  }
  if (!query.exec("SELECT COUNT(*) FROM schema_version") || !query.next()) {
    return fail(query.lastError().text());
  }
  if (query.value(0).toInt() == 0 && !query.exec("INSERT INTO schema_version (version) VALUES (0)")) {
    return fail(query.lastError().text());
  }

  const int current = SchemaVersion();
  if (current > kLatestVersion) {
    return fail(
        QString("Database schema version %1 is newer than supported version %2").arg(current).arg(kLatestVersion));
  }

  for (int version = current + 1; version <= kLatestVersion; ++version) {
    // QSqlQuery runs one statement at a time, so split the migration script.
    const QStringList statements = QString::fromUtf8(kMigrations[version - 1]).split(';', Qt::SkipEmptyParts);
    for (const QString& statement : statements) {
      if (statement.trimmed().isEmpty()) {
        continue;
      }
      if (!query.exec(statement)) {
        return fail(QString("Migration %1 failed: %2").arg(version).arg(query.lastError().text()));
      }
    }
    if (!query.exec(QString("UPDATE schema_version SET version = %1").arg(version))) {
      return fail(query.lastError().text());
    }
  }

  if (!db.commit()) {
    return fail(db.lastError().text());
  }
  return true;
}

}  // namespace sheaf
