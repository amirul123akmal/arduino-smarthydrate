
def delete_user(db, user_id):
    """
    Deletes a user from the database by ID.
    
    Args:
        db: The SqliteDB instance.
        user_id: The ID of the user to delete.
        
    Returns:
        True if successful, False otherwise.
    """
    try:
        query = "DELETE FROM users WHERE id = ?"
        db.execute(query, (user_id,))
        return True
    except Exception as e:
        print(f"Error in delete_user: {e}")
        raise e
