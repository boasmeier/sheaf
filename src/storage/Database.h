#pragma once

#include <QSqlDatabase>
#include <QString>

namespace sheaf {

// Owns the SQLite connection and applies schema migrations.
class Database {
public:
  // Number of the newest schema version known to this build.
  static int LatestSchemaVersion();

  // Default location: <AppDataLocation>/sheaf.db.
  static QString DefaultPath();

  // `connection_name` must be unique per open database in the process.
  explicit Database(QString connection_name);
  ~Database();
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  // Opens (creating if needed) the database file and migrates it to the latest version.
  // Use ":memory:" for an in-memory database. Returns false and sets Error() on failure.
  bool Open(const QString& path);
  void Close();

  bool IsOpen() const;
  int SchemaVersion() const;
  QString Error() const;
  QSqlDatabase Handle() const;

private:
  bool Migrate();

  QString connection_name_;
  QString error_;
};

}  // namespace sheaf
